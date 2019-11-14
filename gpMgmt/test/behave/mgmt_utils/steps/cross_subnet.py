import os
import subprocess

from behave import given, when, then

from gppylib.db import dbconn
from gppylib.gparray import GpArray

# To test that an added mirror/standby works properly, we ensure that we
#   can actually recover data on that segment when it fails over.  We also
#   then bring up the original segment pair to ensure that the new primary/master
#   is fully functional.
#
# We follow these steps:
# 1). add table/data to current master/primary
# 2). stop master/primary
# 3). wait for automatic failover(for downed primary) or explicitly promote standby(for downed master)
# 4). make sure data is on new master/primary
# 5). prepare for 6:
#     - for segments: recoverseg to bring up old primary as mirror
#     - for the master: bring up a new standby
# 6). repeat 1-4 for new-old back to old-new
#
# XXX In addition to the above steps, we manually check to ensure that a
# utility-mode replication connection can also succeed from the mirror host to
# the primary host before and after both failover and recovery.
@then('the {segment} replicates and fails over and back correctly')
@then('the {segment} replicate and fail over and back correctly')
def impl(context, segment):
    if segment not in ('standby', 'mirrors'):
        raise Exception("invalid segment type")

    context.standby_hostname = 'mdw-2'
    context.execute_steps(u"""
    Given the segments are synchronized
     Then replication connections can be made from the acting {segment}

    Given a tablespace is created with data
    """.format(segment=segment))

    # For the 'standby' case, we set PGHOST back to its original value instead
    # of 'mdw-1'.  When the function impl() is called, PGHOST is initially unset
    # by the test framework, and we want to respect that.
    orig_PGHOST = os.environ.get('PGHOST')

    # Fail over to standby/mirrors.
    if segment == 'standby':
        master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        context.standby_port = os.environ.get('PGPORT')
        context.standby_data_dir = master_data_dir
        context.new_standby_data_dir = '%s_1' % master_data_dir
        context.execute_steps(u"""
         When the master goes down
          And the user runs command "gpactivatestandby -a" from standby master
         Then gpactivatestandby should return a return code of 0
         """)
        os.environ['PGHOST'] = 'mdw-2'

    else: # mirrors
        context.execute_steps(u"""
        Given user stops all primary processes
          And user can start transactions
         When the user runs "gprecoverseg -a"
         Then gprecoverseg should return a return code of 0
        """)

    context.execute_steps(u"""
     Then the segments are synchronized
      And the tablespace is valid
      And replication connections can be made from the acting {segment}

    Given another tablespace is created with data
    """.format(segment=segment))

    # Fail over (rebalance) to original master/primaries.
    if segment == 'standby':
        # Re-initialize the standby with a new directory, since
        # the previous master cannot assume the role of standby
        # because it does not have the required recover.conf file.
        context.execute_steps(u"""
         When the user runs command "gpinitstandby -a -s mdw-1 -S {datadir}" from standby master
         Then gpinitstandby should return a return code of 0
         """.format(datadir=context.new_standby_data_dir))
        os.environ['MASTER_DATA_DIRECTORY'] = context.new_standby_data_dir
        # NOTE: this must be set before gpactivatestandby is called
        if orig_PGHOST is None:
            del os.environ['PGHOST']
        else:
            os.environ['PGHOST'] = orig_PGHOST
        context.standby_hostname = 'mdw-1'
        context.execute_steps(u"""
         When the master goes down on "mdw-2"
          And the user runs "gpactivatestandby -a"
         Then gpactivatestandby should return a return code of 0
        """)

        context.execute_steps(u"""
         When the user runs "gpinitstandby -a -s mdw-2 -S {datadir}"
         Then gpinitstandby should return a return code of 0
         """.format(datadir=context.new_standby_data_dir))

    else: # mirrors
        context.execute_steps(u"""
         When the user runs "gprecoverseg -ra"
         Then gprecoverseg should return a return code of 0
        """)

    context.execute_steps(u"""
     Then the segments are synchronized
      And the tablespace is valid
      And the other tablespace is valid
      And replication connections can be made from the acting {segment}
      And all tablespaces are dropped
    """.format(segment=segment))

@then('replication connections can be made from the acting {segment}')
def impl(context, segment):
    if segment not in ('standby', 'mirrors'):
        raise Exception("invalid segment type")

    def check_replication(primary, mirror_hostname):
        # Perform a manual replication connection from the mirror host to the
        # primary host. See
        #     https://www.postgresql.org/docs/9.4/protocol-replication.html
        subprocess.check_call([
            'ssh', '-n', mirror_hostname,
            'PGOPTIONS="-c gp_role=utility"',
            '{gphome}/bin/psql -h {host} -p {port} "dbname=postgres replication=database" -c "IDENTIFY_SYSTEM;"'.format(
                gphome=os.environ['GPHOME'],
                host=primary.address, # use the "internal" routing address
                port=primary.port,
            )
        ])

    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    if segment == 'standby':
        check_replication(gparray.master, context.standby_hostname)
    else: # mirrors
        for pair in gparray.segmentPairs:
            check_replication(pair.primaryDB, pair.mirrorDB.hostname)
