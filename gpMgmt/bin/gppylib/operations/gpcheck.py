import os
from gppylib.commands.base import Command, REMOTE
from gppylib.commands.unix import getLocalHostname

gphome = os.environ.get('GPHOME')

def get_command(local, command, hostname):
    if local:
        cmd = Command(hostname, cmdStr=command)
    else:
        cmd = Command(hostname, cmdStr=command, ctxt=REMOTE, remoteHost=hostname)

    return cmd

def get_host_for_command(local, cmd):
    if local:
        cmd = Command(name='get the hostname', cmdStr='hostname')
        cmd.run(validateAfter=True)
        results = cmd.get_results()
        return results.stdout.strip()
    else:
        return cmd.remoteHost

def get_copy_command(local, host, datafile, tmpdir):
    if local:
        cmd_str = 'mv -f %s %s/%s.data' % (datafile, tmpdir, host)
    else:
        cmd_str = 'scp %s:%s %s/%s.data' % (host, datafile, tmpdir, host)
   
    return Command(host, cmd_str)
