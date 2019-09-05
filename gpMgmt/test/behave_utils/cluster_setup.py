#!/usr/bin/env python
import os
import socket
import inspect
from gppylib.commands.base import Command

class GpDeleteSystem(Command):
    """This is a wrapper for gpdeletesystem."""
    def __init__(self, mdd=None):
        if not mdd:
            mdd = os.getenv('MASTER_DATA_DIRECTORY')
        cmd_str = "export MASTER_DATA_DIRECTORY=%s; echo -e \"y\\ny\\n\" | gpdeletesystem -d %s" % (mdd, mdd)
        Command.__init__(self, 'run gpdeletesystem', cmd_str)

    def run(self, validate=True):
        print "Running delete system: %s" % self
        Command.run(self, validateAfter=validate)
        result = self.get_results()
        return result


class TestCluster:
    def __init__(self, hosts = None, base_dir = '/tmp/default_gpinitsystem', hba_hostnames='0'):
        """
        hosts: lists of cluster hosts. master host will be assumed to be the first element.
        base_dir: cluster directory
        """

        master_host = 'localhost'
        segments_host = socket.gethostname()
        self.hosts = [master_host, segments_host]

        if hosts:
            self.hosts = hosts

        self.port_base = '20500'
        self.master_port = os.environ.get('PGPORT', '10300')
        self.mirror_port_base = '21500'

        self.gpinitconfig_template = local_path('configs/gpinitconfig_template')
        self.gpinitconfig_mirror_template = local_path('configs/gpinitconfig_mirror_template')

        # Output directory specific to this test
        self.base_dir = base_dir

        self.init_file = os.path.join(self.base_dir, 'gpinitconfig')
        self.hosts_file = os.path.join(self.base_dir, 'hosts')
        self.gpexpand_file = os.path.join(self.base_dir, 'gpexpand_input')

        self.primary_dir = os.path.join(self.base_dir, 'data/primary')
        self.mirror_dir = os.path.join(self.base_dir, 'data/mirror')
        self.master_dir = os.path.join(self.base_dir, 'data/master')

        # Test metadata
        # Whether to do gpinitsystem or not
        self.mirror_enabled = False
        self.number_of_segments = 2
        self.number_of_hosts = len(self.hosts)-1

        self.number_of_expansion_hosts = 0
        self.number_of_expansion_segments = 0
        self.number_of_parallel_table_redistributed = 4
        self.hba_hostnames = hba_hostnames

    def _generate_env_file(self):
        env_file = os.path.join(self.base_dir, 'gpdb-env.sh')
        with open(env_file, 'w') as f:
            f.write('#!/usr/bin/env bash\n')
            f.write('export MASTER_DATA_DIRECTORY=%s\n' % os.path.join(self.master_dir,'gpseg-1'))
            f.write('export PGPORT=%s\n' % self.master_port)
            f.flush()

    def _generate_gpinit_config_files(self):
        transforms = {'%PORT_BASE%': self.port_base,
                      '%MASTER_HOST%': self.hosts[0], # First item in self.hosts
                      '%HOSTFILE%': self.hosts_file,
                      '%MASTER_PORT%': self.master_port,
                      '%MASTER_DATA_DIR%': self.master_dir,
                      '%DATA_DIR%': (self.primary_dir + ' ') * self.number_of_segments,
                      '%HBA_HOSTNAMES%': self.hba_hostnames
                      }

        if self.mirror_enabled:
            transforms['%MIRROR_DIR%'] = (self.mirror_dir + ' ') * self.number_of_segments
            transforms['%MIRROR_PORT_BASE%'] = self.mirror_port_base

        # First generate host file based on number_of_hosts
        with open(self.hosts_file, 'w') as f:
            for host in self.hosts[1:self.number_of_hosts+1]:
                f.write(host + '\n')

        config_template = self.gpinitconfig_mirror_template if self.mirror_enabled else self.gpinitconfig_template
        substitute_strings_in_file(config_template, self.init_file, transforms)

    def reset_cluster(self):
        reset_hosts(self.hosts, test_base_dir = self.base_dir)

    def create_cluster(self, with_mirrors=False, mirroring_configuration='group'):
        # Generate the config files to initialize the cluster
        # todo: DATA_DIRECTORY and MIRROR_DATA_DIRECTORY should have only one directory, not 2 when specifying spread
        if mirroring_configuration not in ['group', 'spread']:
            raise Exception('Mirroring configuration must be group or spread')
        self.mirror_enabled = with_mirrors
        self._generate_gpinit_config_files()
        assert os.path.exists(self.init_file)
        assert os.path.exists(self.hosts_file)

        # run gpinitsystem
        clean_env = 'unset MASTER_DATA_DIRECTORY; unset PGPORT;'
        segment_mirroring_option = '--mirror-mode=spread' if mirroring_configuration == 'spread' else ''
        gpinitsystem_cmd = clean_env + 'gpinitsystem -a -c  %s %s' % (self.init_file, segment_mirroring_option)
        res = run_shell_command(gpinitsystem_cmd, 'run gpinitsystem', verbose=True)
        # initsystem returns 1 for warnings and 2 for errors
        if res['rc'] > 1:
            raise Exception("Failed initializing the cluster. Look into gpAdminLogs for more information")
        self._generate_env_file()


###
# HELPER FUNCTIONS
###
def substitute_strings_in_file(input_file, output_file, sub_dictionary):
    """
    This definition will substitute all the occurences of sub_dictionary's 'keys' in
    input_file to dictionary's 'values' to create the output_file.

    @type input_file: string
    @param input_file: Absolute path of the file to read.

    @type output_file: string
    @param output_file: Absolute path of the file to create.

    @type sub_dictionary: dictionary @param sub_dictionary: Dictionary that specifies substitution. Key will be replaced with Value.  @rtype bool @returns True if there is at least one substitution made , False otherwise """
    print "input_file: %s ; output_file: %s ; sub_dictionary: %s" % (input_file, output_file, str(sub_dictionary))
    substituted = False
    with open(output_file, 'w') as output_file_object:
        with open(input_file, 'r') as input_file_object:
            for each_line in input_file_object:
                new_line = each_line
                for key,value in sub_dictionary.items():
                    new_line = new_line.replace(key, value)
                if not each_line == new_line:
                    substituted = True
                output_file_object.write(new_line)
    print "Substituted: %s" % str(substituted)
    return substituted

def local_path(filename):
    """Return the absolute path of the input file."""
    frame = inspect.stack()[1]
    source_file = inspect.getsourcefile(frame[0])
    source_dir = os.path.dirname(source_file)
    return os.path.join(source_dir, filename)

def run_shell_command(cmdstr, cmdname = 'shell command', results={'rc':0, 'stdout':'', 'stderr':''}, verbose=False):
    cmd = Command(cmdname, cmdstr)
    cmd.run()
    result = cmd.get_results()
    results['rc'] = result.rc
    results['stdout'] = result.stdout
    results['stderr'] = result.stderr

    if verbose:
        print "command output: %s" % results['stdout']
    if results['rc'] != 0:
        if verbose:
            print "command error: %s" % results['stderr']
    return results

def reset_hosts(hosts, test_base_dir):

    primary_dir = os.path.join(test_base_dir, 'data', 'primary')
    mirror_dir = os.path.join(test_base_dir, 'data', 'mirror')
    master_dir = os.path.join(test_base_dir, 'data', 'master')

    host_args = " ".join(map(lambda x: "-h %s" % x, hosts))
    reset_primary_dirs_cmd = "gpssh %s -e 'rm -rf %s; mkdir -p %s'" % (host_args, primary_dir, primary_dir)
    res = run_shell_command(reset_primary_dirs_cmd, 'reset segment dirs', verbose=True)
    if res['rc'] > 0:
        raise Exception("Failed to reset segment directories")

    reset_mirror_dirs_cmd = "gpssh %s -e 'rm -rf %s; mkdir -p %s'" % (host_args, mirror_dir, mirror_dir)
    res = run_shell_command(reset_mirror_dirs_cmd, 'reset segment dirs', verbose=True)
    if res['rc'] > 0:
        raise Exception("Failed to reset segment directories")

    reset_master_dirs_cmd = "gpssh %s -e 'rm -rf %s; mkdir -p %s'" % (host_args, master_dir, master_dir)
    res = run_shell_command(reset_master_dirs_cmd, 'reset segment dirs', verbose=True)
    if res['rc'] > 0:
        raise Exception("Failed to reset segment directories")


###
# MAIN
###
if __name__ == '__main__':
    # Trial on how we can use this
    testcluster = TestCluster([])
    testcluster.reset_cluster()
    testcluster.create_cluster()
