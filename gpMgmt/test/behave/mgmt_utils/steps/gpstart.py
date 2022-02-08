import os
import signal
import subprocess
import time

from contextlib import closing

from behave import given, when, then
from test.behave_utils import utils
from test.behave_utils.utils import wait_for_unblocked_transactions
from gppylib.commands.base import Command
from gppylib.db import dbconn

def _run_sql(sql, opts=None):
    env = None

    if opts is not None:
        env = os.environ.copy()

        options = ''
        for key, value in opts.items():
            options += "-c {}={} ".format(key, value)

        env['PGOPTIONS'] = options

    subprocess.check_call([
        "psql",
        "postgres",
        "-c", sql,
    ], env=env)

def change_hostname(content, preferred_role, hostname):
    with closing(dbconn.connect(dbconn.DbURL(dbname="template1"), allowSystemTableMods=True, unsetSearchPath=False)) as conn:
        dbconn.execSQL(conn, "UPDATE gp_segment_configuration SET hostname = '{0}', address = '{0}' WHERE content = {1} AND preferred_role = '{2}'".format(hostname, content, preferred_role))

@when('the standby host is made unreachable')
def impl(context):
    change_hostname(-1, 'm', 'invalid_host')

    context.add_cleanup(cleanup, context)

@when('the standby host is made reachable')
@then('the standby host is made reachable')
def impl(context):
    cleanup(context)

"""
Reverses the changes done by change_hostname() function by starting up cluster in master-only utility mode. 
Since the standby host is incorrect, a regular gpstart call won't work.
"""
def cleanup(context):

    utils.stop_database_if_started(context)

    subprocess.check_call(['gpstart', '-am'])
    _run_sql("""
        SET allow_system_table_mods='true';
        UPDATE gp_segment_configuration
           SET hostname = coordinator.hostname,
                address = coordinator.address
          FROM (
                 SELECT hostname, address
                   FROM gp_segment_configuration
                  WHERE content = -1 and role = 'p'
               ) coordinator
         WHERE content = -1 AND role = 'm'
    """, {'gp_role': 'utility'})
    subprocess.check_call(['gpstop', '-am'])

def _handle_sigpipe():
    """
    Work around https://bugs.python.org/issue1615376, which is not fixed until
    Python 3.2. This bug interferes with Bash pipelines that rely on SIGPIPE to
    exit cleanly.
    """
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

@when('"{cmd}" is run with prompts accepted')
def impl(context, cmd):
    """
    Runs `yes | cmd`.
    """

    p = subprocess.Popen(
        ["bash", "-c", "yes | %s" % cmd],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=_handle_sigpipe,
    )

    context.stdout_message, context.stderr_message = p.communicate()
    context.ret_code = p.returncode

@given('the host for the {seg_type} on content {content} is made unreachable')
def impl(context, seg_type, content):
    if seg_type == "primary":
        preferred_role = 'p'
    elif seg_type == "mirror":
        preferred_role = 'm'
    else:
        raise Exception("Invalid segment type %s (options are primary and mirror)" % seg_type)

    with closing(dbconn.connect(dbconn.DbURL(dbname="template1"), unsetSearchPath=False)) as conn:
        dbid, hostname = dbconn.queryRow(conn, "SELECT dbid, hostname FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (content, preferred_role))
    if not hasattr(context, 'old_hostnames'):
        context.old_hostnames = {}
    context.old_hostnames[(content, preferred_role)] = hostname
    change_hostname(content, preferred_role, 'invalid_host')

    if not hasattr(context, 'down_segment_dbids'):
        context.down_segment_dbids = []
    context.down_segment_dbids.append(dbid)

    wait_for_unblocked_transactions(context)

@then('gpstart should print unreachable host messages for the down segments')
def impl(context):
    if not hasattr(context, 'down_segment_dbids'):
        raise Exception("Cannot check messages for down segments: no dbids are saved")
    for dbid in sorted(context.down_segment_dbids):
        context.execute_steps(u'Then gpstart should print "Marking segment %s down because invalid_host is unreachable" to stdout' % dbid)

def has_expected_status(content, preferred_role, expected_status):
    with closing(dbconn.connect(dbconn.DbURL(dbname="template1"), unsetSearchPath=False)) as conn:
        status = dbconn.querySingleton(conn, "SELECT status FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (content, preferred_role))
    return status == expected_status


def must_have_expected_status(content, preferred_role, expected_status):
    with closing(dbconn.connect(dbconn.DbURL(dbname="template1"), unsetSearchPath=False)) as conn:
        status = dbconn.querySingleton(conn, "SELECT status FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (content, preferred_role))
    if status != expected_status:
        raise Exception("Expected status for role %s to be %s, but it is %s" % (preferred_role, expected_status, status))

def get_guc_value(guc):
    with closing(dbconn.connect(dbconn.DbURL(dbname="template1"), unsetSearchPath=False)) as conn:
        value = dbconn.querySingleton(conn, "show %s" % guc)
    return value

def set_guc_value(context, guc, value):
    context.execute_steps(u'''
        When "gpconfig -c %s -v %s" is run with prompts accepted
        Then gpconfig should return a return code of 0
        When the user runs "gpstop -u"
    ''' % (guc, value))

# this can be done to have the test run faster...
# gpconfig -c gp_fts_mark_mirror_down_grace_period -v 5
# postgres=# show gp_fts_mark_mirror_down_grace_period;
@given('the status of the {seg_type} on content {content} should be "{expected_status}"')
@then('the status of the {seg_type} on content {content} should be "{expected_status}"')
def impl(context, seg_type, content, expected_status):
    if seg_type == "primary":
        preferred_role = 'p'
    elif seg_type == "mirror":
        preferred_role = 'm'
    else:
        raise Exception("Invalid segment type %s (options are primary and mirror)" % seg_type)

    # it takes gp_fts_mark_mirror_down_grace_period seconds between fts probes
    # for fts to mark a mirror as down.  That guc is not settable per session;
    # it must be changed for the database.
    timeout = 3
    if preferred_role == 'm':
        orig = get_guc_value("gp_fts_mark_mirror_down_grace_period")
        set_guc_value(context, "gp_fts_mark_mirror_down_grace_period", timeout)
        utils.trigger_fts_probe()
        for i in range(1,4):
            time.sleep(timeout + 1) # Sleep an extra second since the timing isn't necessarily exact
            utils.trigger_fts_probe()
            if has_expected_status(content, preferred_role, expected_status):
                break
        set_guc_value(context, "gp_fts_mark_mirror_down_grace_period", orig)

    must_have_expected_status(content, preferred_role, expected_status)

@given('the cluster is returned to a good state')
@then('the cluster is returned to a good state')
def impl(context):
    if not hasattr(context, 'old_hostnames'):
        raise Exception("Cannot reset segment hostnames: no hostnames are saved")
    for key, hostname in context.old_hostnames.items():
        change_hostname(key[0], key[1], hostname)

    context.execute_steps("""
    When the user runs "gprecoverseg -a"
    Then gprecoverseg should return a return code of 0
    And all the segments are running
    And the segments are synchronized
    When the user runs "gprecoverseg -a -r"
    Then gprecoverseg should return a return code of 0
    And all the segments are running
    And the segments are synchronized
    """)
