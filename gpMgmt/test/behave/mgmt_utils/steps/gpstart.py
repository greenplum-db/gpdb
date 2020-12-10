import os
import signal
import subprocess

from behave import given, when, then
from test.behave_utils import utils
from test.behave_utils.utils import wait_for_unblocked_transactions
from gppylib.db import dbconn

@given('the temporary filespace is moved')
def impl(context):
    context.execute_steps(u'''
        Given a filespace_config_file for filespace "tempfs" is created using config file "tempfs_config" in directory "/tmp"
          And a filespace is created using config file "tempfs_config" in directory "/tmp"
          And the user runs "gpfilespace --movetempfilespace tempfs"
    ''')

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
    with dbconn.connect(dbconn.DbURL(dbname="template1"), allowSystemTableMods='dml') as conn:
        dbconn.execSQL(conn, "UPDATE gp_segment_configuration SET hostname = '{0}', address = '{0}' WHERE content = {1} AND preferred_role = '{2}'".format(hostname, content, preferred_role))
        conn.commit()

@when('the standby host is made unreachable')
def impl(context):
    change_hostname(-1, 'm', 'invalid_host')

    def cleanup(context):
        """
        Reverses the above SQL by starting up in master-only utility mode. Since
        the standby host is incorrect, a regular gpstart call won't work.
        """
        utils.stop_database_if_started(context)

        subprocess.check_call(['gpstart', '-am'])
        _run_sql("""
            SET allow_system_table_mods='dml';
            UPDATE gp_segment_configuration
               SET hostname = master.hostname,
                    address = master.address
              FROM (
                     SELECT hostname, address
                       FROM gp_segment_configuration
                      WHERE content = -1 and role = 'p'
                   ) master
             WHERE content = -1 AND role = 'm'
        """, {'gp_session_role': 'utility'})
        subprocess.check_call(['gpstop', '-am'])

    context.cleanup_standby_host_failure = cleanup

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

    with dbconn.connect(dbconn.DbURL(dbname="template1")) as conn:
        dbid, hostname = dbconn.execSQLForSingletonRow(conn, "SELECT dbid, hostname FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (content, preferred_role))
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

def must_have_expected_field(content, preferred_role, field, expected):
    with dbconn.connect(dbconn.DbURL(dbname="template1")) as conn:
        result = dbconn.execSQLForSingleton(conn, "SELECT %s FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (field, content, preferred_role))
    if result != expected:
        raise Exception("Expected %s for role %s to be %s, but it is %s" % (field, preferred_role, expected, result))

@then('the {field} of the {seg_type} on content {content} should be "{expected}"')
def impl(context, field, seg_type, content, expected):
    if seg_type == "primary":
        preferred_role = 'p'
    elif seg_type == "mirror":
        preferred_role = 'm'
    else:
        raise Exception("Invalid segment type %s (options are primary and mirror)" % seg_type)

    wait_for_unblocked_transactions(context)

    must_have_expected_field(content, preferred_role, field, expected)

@then('the cluster is returned to a good state')
def impl(context):
    if not hasattr(context, 'old_hostnames'):
        raise Exception("Cannot reset segment hostnames: no hostnames are saved")
    for key, hostname in context.old_hostnames.items():
        change_hostname(key[0], key[1], hostname)

    context.execute_steps(u"""
    When the user runs "gprecoverseg -a"
    Then gprecoverseg should return a return code of 0
    And all the segments are running
    And the segments are synchronized
    When the user runs "gprecoverseg -a -r"
    Then gprecoverseg should return a return code of 0
    And all the segments are running
    And the segments are synchronized
    """)
