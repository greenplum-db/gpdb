from os import path
import os
import shutil
import socket
try:
    import subprocess32 as subprocess
except:
    import subprocess
import sys
import tempfile

import pipes

from behave import given, when, then
from test.behave_utils.utils import *

from mgmt_utils import *

class GpsshExkeysMgmtContext:
    """
    This class is intended to store per-Scenario state that is built up over a
    series of steps.
    """
    def __init__(self, context):
        self.master_host = None
        self.segment_hosts = None
        make_temp_dir(context, '/tmp/gpssh-exkeys', '0700')
        self.working_directory = context.temp_base_dir

    def allHosts(self):
        allHosts = [self.master_host]
        allHosts.extend(self.segment_hosts)
        return allHosts


@given('the gpssh-exkeys master host is set to "{host}"')
def impl(context, host):
    context.gpssh_exkeys_context.master_host = host

@given('the gpssh-exkeys segment host is set to "{hosts}"')
def impl(context, hosts):
    context.gpssh_exkeys_context.segment_hosts = [ h.strip() for h in hosts.split(',') ]

def run_exkeys(hosts, capture=False):
    """
    Runs gpssh-exkeys for the given list of hosts. If capture is True, the
    (returncode, stdout, stderr) from the gpssh-exkeys run is returned;
    otherwise an exception is thrown on failure and all stdout/err is untouched.
    """
    host_opts = []
    for host in hosts:
        host_opts.extend(['-h', host])

    args = [ 'gpssh-exkeys', '-v' ] + host_opts

    if not capture:
        subprocess.check_call(args)
        return

    # Capture stdout/err for later use, while routing it through tee(1) so that
    # developers can still see the live stream output.
    #
    # XXX This is a very heavy-weight solution, using pipes.Template() for the
    # creation of shell pipeline processes. It's also platform-specific as it
    # relies on the functionality of /dev/stdout and /dev/stderr.
    #
    # The overview: we open up two shell processes running tee(1), using
    # pipes.Template(), and connect their standard output to the stdout/err of
    # the current Python process using Template.open(). We then connect the
    # stdout/stderr streams of subprocess.call() to the stdin of those tee
    # pipelines. tee(1) will duplicate all output to temporary files, which we
    # read after the subprocess call completes. NamedTemporaryFile() then cleans
    # up those files when we return.
    with tempfile.NamedTemporaryFile() as temp_out, tempfile.NamedTemporaryFile() as temp_err:
        pipe_out = pipes.Template()
        pipe_out.append('tee %s' % pipes.quote(temp_out.name), '--')

        pipe_err = pipes.Template()
        pipe_err.append('tee %s' % pipes.quote(temp_err.name), '--')

        with pipe_out.open('/dev/stdout', 'w') as out, pipe_err.open('/dev/stderr', 'w') as err:
            ret = subprocess.call(args, stdout=out, stderr=err)

        stored_out = temp_out.read()
        stored_err = temp_err.read()

    return ret, stored_out, stored_err

@then('gpssh-exkeys writes "{output}" to stderr')
def impl(context, output):
    if 'stderr' not in context:
        raise Exception('context has no stored stderr (did you run the correct steps?)')

    if output not in context.stderr:
        msg = 'expected stderr content not found. stderr:\n%s' % context.stderr
        raise Exception(msg)

@when('gpssh-exkeys is run')
def impl(context):
    hosts = context.gpssh_exkeys_context.allHosts()
    code, stdout, stderr = run_exkeys(hosts, capture=True)
    context.ret_code = code
    context.stdout = stdout
    context.stderr = stderr

@when('gpssh-exkeys is run successfully')
def impl(context):
    run_exkeys(context.gpssh_exkeys_context.allHosts())

@given('gpssh-exkeys is run successfully on hosts "{hosts}"')
@when('gpssh-exkeys is run successfully on hosts "{hosts}"')
def impl(context, hosts):
    run_exkeys([ h.strip() for h in hosts.split(',') ])

@when('gpssh-exkeys is run successfully on additional hosts "{new_hosts}"')
def impl(context, new_hosts):
    new_hosts = [ h.strip() for h in new_hosts.split(',') ]
    old_hosts = [
        h for h in context.gpssh_exkeys_context.allHosts() if h not in new_hosts
    ]

    old_host_file = tempfile.NamedTemporaryFile()
    new_host_file = tempfile.NamedTemporaryFile()

    with old_host_file, new_host_file:
        for h in old_hosts:
            old_host_file.write(h + '\n')
        old_host_file.flush()

        for h in new_hosts:
            new_host_file.write(h + '\n')
        new_host_file.flush()

        subprocess.check_call([
            'gpssh-exkeys',
            '-v',
            '-e', old_host_file.name,
            '-x', new_host_file.name,
        ])

@when('gpssh-exkeys is run successfully with a hostfile')
def impl(context):
    with tempfile.NamedTemporaryFile() as host_file:
        for h in context.gpssh_exkeys_context.allHosts():
            host_file.write(h + '\n')
        host_file.flush()

        subprocess.check_call([
            'gpssh-exkeys',
            '-v',
            '-f', host_file.name,
        ])

@when('gpssh-exkeys is run successfully with IPv6 addresses')
def impl(context):
    ipv6_addrs = []
    for host in context.gpssh_exkeys_context.allHosts():
        # Try to look up an IPv6 address for each host.
        try:
            addrs = socket.getaddrinfo(host, None, socket.AF_INET6)
        except socket.gaierror as err:
            raise Exception, \
                "failed to find IPv6 address for host '{}': {}".format(host, err), \
                sys.exc_info()[2]

        # getaddrinfo() return value is a bit opaque. For AF_INET6, it's a list
        # of (family, socktype, proto, canonname, (address, port, flow info, scope id))
        # nested tuples. We're interested in the address piece of the first
        # entry in that list.
        addr = addrs[0][4][0]
        print host, "maps to", addr

        ipv6_addrs.append(addr)

    run_exkeys(ipv6_addrs)


@then('all hosts "{works}" reach each other or themselves automatically')
def impl(context, works):
    steps = u'''
    Then the segment hosts "{0}" reach each other or themselves automatically
     And the segment hosts "{0}" reach the master
     And the master host "{0}" reach itself
    '''.format(works)
    context.execute_steps(steps)


# TODO: we are currently not using gpssh so we can control StrictHostKeyChecking=yes
@then('the segment hosts "{works}" reach each other or themselves automatically')
def impl(context, works):
    ret = 255
    if (works == 'can'):
        ret = 0
    # NOTE: we tried using scp with files instead, but -o BatchMode=yes -o StrictHostKeyChecking=yes
    # still asked us for a prompt.
    # we're not using gpssh here because we want to test each connection
    for fromHost in context.gpssh_exkeys_context.segment_hosts:
        for toHost in context.gpssh_exkeys_context.segment_hosts:
            cmd = u'''
            When the user runs command "ssh -o BatchMode=yes -o StrictHostKeyChecking=yes %s \"ssh -o BatchMode=yes -o StrictHostKeyChecking=yes %s hostname\"" eok
            And ssh should return a return code of %d
            ''' % (fromHost, toHost, ret)
            print "CMD:%s" % cmd
            context.execute_steps(cmd)


@then('the segment hosts "{works}" reach the master')
def impl(context, works):
    host_opts = []
    for host in context.gpssh_exkeys_context.segment_hosts:
        host_opts.extend(['-h', host])

    subprocess.check_call([
        'gpssh',
        '-e',
        ] + host_opts + [
        '{}ssh -o BatchMode=yes -o StrictHostKeyChecking=yes mdw true'.format(
            "" if (works == 'can') else "! "
        )
    ])


@then('the master host "{works}" reach itself')
def impl(context, works):
    result = subprocess.call(['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', 'mdw', 'true'])
    should_work = (works == 'can')
    did_work = (result == 0)
    if should_work != did_work:
        expected_code = '0' if should_work else 'not 0'
        raise Exception('actual result of ssh mdw: %s (expected: %s)', result, expected_code)


@given('all SSH configurations are backed up and stripped')
def impl(context):
    """
    Strips out part of the ssh secrets to setup the cluster so only ssh from
    the local to the remotes works.
    """
    host_opts = []
    for host in context.gpssh_exkeys_context.segment_hosts:
        host_opts.extend(['-h', host])

    # Everything except authorized_keys is moved elsewhere.
    subprocess.check_call([
        'gpssh',
        '-e',
        ] + host_opts + [(
        'mkdir -p /tmp/ssh.bak '
        '&& mv -f ~/.ssh/* /tmp/ssh.bak '
        '&& cp -fp /tmp/ssh.bak/authorized_keys ~/.ssh/'
    )])

    # Also backup .ssh on mdw, leaving the key configuration in .ssh
    home_ssh = path.expanduser('~/.ssh')
    backup_path = '/tmp/ssh.bak/'
    os.makedirs(backup_path)
    for ssh_file in os.listdir(home_ssh):
        if not ssh_file.startswith('id_rsa'):
            shutil.move(path.join(home_ssh, ssh_file), backup_path)

    # Make sure the configuration is restored at the end.
    def cleanup():
        subprocess.check_call([
            'gpssh',
            '-e',
            ] + host_opts + [
            'mv -f /tmp/ssh.bak/* ~/.ssh/',
        ])
        for f in os.listdir(backup_path):
            shutil.move(path.join(backup_path, f), path.join(home_ssh, f))
        os.rmdir(backup_path)

    context.add_cleanup(cleanup)

@given('the local SSH configuration is backed up and removed')
def impl(context):
    """
    Strips out part of the ssh secrets to setup the cluster so only ssh from
    the local to the remotes works.
    """
    # Also backup .ssh on mdw, leaving the key configuration in .ssh
    home_ssh = path.expanduser('~/.ssh')
    backup_path = tempfile.mkdtemp()
    for ssh_file in os.listdir(home_ssh):
        shutil.move(path.join(home_ssh, ssh_file), backup_path)

    # Make sure the configuration is restored at the end.
    def cleanup():
        for f in os.listdir(backup_path):
            shutil.move(path.join(backup_path, f), path.join(home_ssh, f))
        os.rmdir(backup_path)

    context.add_cleanup(cleanup)

@given('the segments can only be accessed using the master key')
def impl(context):
    host_opts = []
    for host in context.gpssh_exkeys_context.segment_hosts:
        host_opts.extend(['-h', host])

    # This blows away any existing authorized_keys file on the segments.
    subprocess.check_call([
        'gpscp',
        '-v',
        ] + host_opts + [
        '~/.ssh/id_rsa.pub',
        '=:~/.ssh/authorized_keys'
    ])

@given('there is no duplication in the "{ssh_type}" files')
@then('there is no duplication in the "{ssh_type}" files')
def impl(context, ssh_type):
    host_opts = []
    for host in context.gpssh_exkeys_context.segment_hosts:
        host_opts.extend(['-h', host])

    # ssh'ing to localhost need not be set up yet
    subprocess.check_call([ 'bash', '-c', '! sort %s | uniq -d | grep .' % path.join('~/.ssh',pipes.quote(ssh_type))])

    subprocess.check_call([
        'gpssh',
        '-e',
        ] + host_opts + [
        '! sort %s | uniq -d | grep .' % path.join('~/.ssh',pipes.quote(ssh_type))
    ])
