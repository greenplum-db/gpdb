import os
import signal
import subprocess

from behave import given, when, then
from  test.behave_utils import utils

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

@when('the standby host goes down')
def impl(context):
    """
    Fakes a host failure by updating the standby segment entry to point at an
    invalid hostname and address.
    """
    opts = {
        'gp_role': 'utility',
        'allow_system_table_mods': 'on',
    }

    subprocess.check_call(['gpstart', '-am'])
    _run_sql("""
        UPDATE gp_segment_configuration
           SET hostname = 'standby.invalid',
                address = 'standby.invalid'
         WHERE content = -1 AND role = 'm'
    """, opts=opts)
    subprocess.check_call(['gpstop', '-am'])

    def cleanup(context):
        """
        Reverses the above SQL by starting up in master-only utility mode. Since
        the standby host is incorrect, a regular gpstart call won't work.
        """

        utils.stop_database_if_started(context)

        subprocess.check_call(['gpstart', '-am'])
        _run_sql("""
            UPDATE gp_segment_configuration
               SET hostname = master.hostname,
                    address = master.address
              FROM (
                     SELECT hostname, address
                       FROM gp_segment_configuration
                      WHERE content = -1 and role = 'p'
                   ) master
             WHERE content = -1 AND role = 'm'
        """, opts=opts)
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
