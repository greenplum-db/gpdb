import os
import signal
import subprocess

from behave import given, when, then
from test.behave_utils import utils
from gppylib.commands.base import Command

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

def do_catalog_query(query):
    cmd = '''PGOPTIONS='-c gp_role=utility' psql -t -d template1 -c "SET allow_system_table_mods='true'; %s"''' % query
    cmd = Command(name="catalog query", cmdStr=cmd)
    cmd.run(validateAfter=True)
    return cmd

def change_hostname(dbid, hostname):
    do_catalog_query("UPDATE gp_segment_configuration SET hostname = '{0}', address = '{0}' WHERE dbid = {1}".format(hostname, dbid))

def change_status(dbid, status):
    do_catalog_query("UPDATE gp_segment_configuration SET status = '%s' WHERE dbid = %s" % (status, dbid))

@when('the standby host goes down')
def impl(context):
    result = do_catalog_query("SELECT dbid FROM gp_segment_configuration WHERE content = -1 AND role = 'm'")
    dbid = result.get_stdout().strip()
    change_hostname(dbid, 'invalid_host')

    def cleanup(context):
        """
        Reverses the above SQL by starting up in master-only utility mode. Since
        the standby host is incorrect, a regular gpstart call won't work.
        """
        utils.stop_database_if_started(context)

        subprocess.check_call(['gpstart', '-am'])
        _run_sql("""
            SET allow_system_table_mods='true';
            UPDATE gp_segment_configuration
               SET hostname = master.hostname,
                    address = master.address
              FROM (
                     SELECT hostname, address
                       FROM gp_segment_configuration
                      WHERE content = -1 and role = 'p'
                   ) master
             WHERE content = -1 AND role = 'm'
        """, {'gp_role': 'utility'})
        subprocess.check_call(['gpstop', '-am'])

    context.add_cleanup(cleanup, context)

def _handle_sigpipe():
    """
    Work around https://bugs.python.org/issue1615376, which is not fixed until
    Python 3.2. This bug interferes with Bash pipelines that rely on SIGPIPE to
    exit cleanly.
    """
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

@when('gpstart is run with prompts accepted')
def impl(context):
    """
    Runs `yes | gpstart`.
    """

    p = subprocess.Popen(
        [ "bash", "-c", "yes | gpstart" ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=_handle_sigpipe,
    )

    context.stdout_message, context.stderr_message = p.communicate()
    context.ret_code = p.returncode

@given('segment {dbid} goes down' )
def impl(context, dbid):
    result = do_catalog_query("SELECT hostname FROM gp_segment_configuration WHERE dbid = %s" % dbid)
    if not hasattr(context, 'old_hostnames'):
        context.old_hostnames = {}
    context.old_hostnames[dbid] = result.get_stdout().strip()
    change_hostname(dbid, 'invalid_host')

@then('the status of segment {dbid} should be "{expected_status}"' )
def impl(context, dbid, expected_status):
    result = do_catalog_query("SELECT status FROM gp_segment_configuration WHERE dbid = %s" % dbid)

    status = result .get_stdout().strip()
    if status != expected_status:
        raise Exception("Expected status to be %s, but it is %s" % (expected_status, status))

@then('the status of segment {dbid} is changed to "{status}"' )
def impl(context, dbid, status):
    do_catalog_query("UPDATE gp_segment_configuration SET status = '%s' WHERE dbid = %s" % (status, dbid))

@then('the cluster is returned to a good state' )
def impl(context):
    if not hasattr(context, 'old_hostnames'):
        raise Exception("Cannot reset segment hostnames: no hostnames are saved")
    for dbid, hostname in context.old_hostnames.items():
        change_hostname(dbid, hostname)

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
