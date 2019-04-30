from os import path
import os
import shutil
import subprocess
import tempfile

import pipes

from behave import given, when, then
from test.behave_utils.utils import *

from mgmt_utils import *

# TODO: if successful, this test will leak small files onto the segment hosts

# This class is intended to store per-Scenario state that is built up over
# a series of steps.


class GpsshExkeysMgmtContext:

    def __init__(self, context):
        self.master_host = None
        self.segment_hosts = None
        self.private_key_file_from = None
        self.private_key_file_to = None
        make_temp_dir(context, '/tmp/gpssh_exkeys', '0700')
        self.working_directory = context.temp_base_dir

    def inputsSetup(self):
        return self.master_host and self.segment_hosts

    def allHosts(self):
        allHosts = [self.master_host]
        allHosts.extend(self.segment_hosts)
        return allHosts


@given('the gpssh_exkeys master host is set to "{host}"')
def impl(context, host):
    context.gpssh_exkeys_context.master_host = host

@given('the gpssh_exkeys segment host is set to "{hosts}"')
def impl(context, hosts):
    context.gpssh_exkeys_context.segment_hosts = hosts.split(',')

@given('the segment known_hosts mapping is removed on localhost')
def impl(context):
    for host in context.gpssh_exkeys_context.allHosts():
        cmd = u'''
       Given the user runs command "ssh-keygen -R %s"
       And ssh-keygen should return a return code of 0
       ''' % host
        context.execute_steps(cmd)

def run_exkeys(hosts):
    host_opts = []
    for host in hosts:
        host_opts.extend(['-h', host])

    subprocess.check_call([
        'gpssh-exkeys',
        '-v',
    ] + host_opts)

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

@when('gpssh-exkeys is run eok')
def impl(context):
    hostsStr = " ".join(["-h %s" % host for host in context.gpssh_exkeys_context.segment_hosts])
    cmd = u'''
    Given the user runs command "gpssh-exkeys -v -h %s %s" eok
    ''' % (context.gpssh_exkeys_context.master_host, hostsStr)
    context.execute_steps(cmd)

# keep 1-N and remove N-N...the master has each segment remove each mapping
# TODO: we are currently not using gpssh so we can control StrictHostKeyChecking=yes
@given('the segment known_hosts mapping is removed')
def impl(context):
    for fromHost in context.gpssh_exkeys_context.segment_hosts:
        for toHost in context.gpssh_exkeys_context.segment_hosts:
            toHostIP = socket.gethostbyname(toHost)
            cmd = u'''
            Given the user runs command "ssh -o BatchMode=yes -o StrictHostKeyChecking=yes %s \"ssh-keygen -R %s\"" eok
            And ssh should return a return code of 0
            Given the user runs command "ssh -o BatchMode=yes -o StrictHostKeyChecking=yes %s \"ssh-keygen -R %s\"" eok
            And ssh should return a return code of 0
            ''' % (fromHost, toHostIP, fromHost, toHost)

            context.execute_steps(cmd)


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
    # TODO: deduplicate
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


@given('the ssh file "{file}" is moved to a temporary directory')
def impl(context, file):
    user_home = os.environ.get('HOME')
    private_key_file = '%s/.ssh/%s' % (user_home, file)
    temporary_file_path = '%s/%s' % (context.gpssh_exkeys_context.working_directory, file)
    shutil.move(private_key_file, temporary_file_path)
    context.gpssh_exkeys_context.private_key_file_from = temporary_file_path
    context.gpssh_exkeys_context.private_key_file_to = private_key_file


@given('all SSH configurations are backed up and removed')
def impl(context):
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


@given('the local public key is backed up and removed')
def impl(context):
    pubkey_path = path.expanduser('~/.ssh/id_rsa.pub')
    backup_path = '/tmp/id_rsa.pub.bak'

    shutil.move(pubkey_path, backup_path)

    # Make sure the key is restored at the end.
    def cleanup():
        shutil.move(backup_path, pubkey_path)
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
