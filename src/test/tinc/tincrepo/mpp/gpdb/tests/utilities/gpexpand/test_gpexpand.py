"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import getpass
import os

import tinctest

from mpp.lib.config import GPDBConfig
from mpp.lib.gpstop import GpStop
from mpp.models import MPPTestCase

from tinctest.models.scenario import ScenarioTestCase
from tinctest.lib.system import TINCSystem
from tinctest.lib import local_path, run_shell_command
from tinctest.main import TINCException

from tinctest.loader import TINCTestLoader

from gppylib.commands.base import Command, REMOTE
from gppylib.db import dbconn
from gppylib.db.dbconn import UnexpectedRowsError
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.PSQL import PSQL

class GpInitSystem(Command):
    """This is a wrapper for gpinitsystem."""
    def __init__(self, config_file):
        cmd_str = 'gpinitsystem -a -c  %s' % (config_file)
        Command.__init__(self, 'run gpinitsystem', cmd_str)

    def run(self, validate=True):
        tinctest.logger.info("Running initialization: %s" %self)
        Command.run(self, validateAfter=validate)
        result = self.get_results()
        return result

class GpInitStandby(Command):
    """This is a wrapper for gpinitstandby."""
    def __init__(self, standby_host, mdd=None):
        if not mdd:
            mdd = os.getenv('MASTER_DATA_DIRECTORY')
        cmd_str = 'export MASTER_DATA_DIRECTORY=%s; gpinitstandby -a -s  %s' % (mdd, standby_host)
        Command.__init__(self, 'run gpinitstandby', cmd_str)

    def run(self, validate=True):
        tinctest.logger.info("Running gpinitstandby: %s" %self)
        Command.run(self, validateAfter=validate)
        result = self.get_results()
        return result

class GpSegInstall(Command):
    """
    This is a wrapper for gpseginstall
    """
    def __init__(self, gphome, hosts):
        self.hostfile = '/tmp/gpseginstall_hosts'
        self.gphome = gphome
        self.hosts = hosts
        cmd_str = "gpseginstall -f %s -u %s" %(self.hostfile, getpass.getuser())
        Command.__init__(self, 'run gpseginstall', cmd_str)

    def run(self, validate=True):
        tinctest.logger.info("Running gpseginstall: %s" %self)
        with open(self.hostfile, 'w') as f:
            for host in self.hosts[1:]:
                f.write(host)
                f.write('\n')
        
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command('gpssh-exkeys -f %s' %self.hostfile, 'gpssh-exkeys', res)

        if res['rc'] > 0:
            raise Exception("Failed to do gpssh-exkeys: %s" %res['stderr'])

        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command("gpssh -f %s -e 'mkdir -p %s'" %(self.hostfile, self.gphome), 'gpssh-exkeys', res)
        if res['rc'] > 0:
            raise Exception("Failed to create gphome directories on segments: %s" %res[stderr])
        
        Command.run(self, validateAfter=validate)
        result = self.get_results()
        return result

class GpDeleteSystem(Command):
    """This is a wrapper for gpdeletesystem."""
    def __init__(self, mdd=None):
        if not mdd:
            mdd = os.getenv('MASTER_DATA_DIRECTORY')
        cmd_str = "export MASTER_DATA_DIRECTORY=%s; echo -e \"y\\ny\\n\" | gpdeletesystem -d %s" % (mdd, mdd)
        Command.__init__(self, 'run gpdeletesystem', cmd_str)
        
    def run(self, validate=True):
        tinctest.logger.info("Running delete system: %s" %self)
        Command.run(self, validateAfter=validate)
        result = self.get_results()
        return result
                                    
class GPExpandTestCaseException(TINCException):
    pass

def get_gpexpand_hosts():
    hosts = []
    if not 'HOST1' in os.environ or not 'HOST2' in os.environ or not 'HOST3' in os.environ or not 'HOST4' in os.environ or not 'HOST5' in os.environ:
        raise GPExpandTestCaseException("gpexpand tests require the environment variables HOST1/HOST2/HOST3/HOST4/HOST5 to be set")

    hosts.append(os.environ.get('HOST1').strip())
    hosts.append(os.environ.get('HOST2').strip())
    hosts.append(os.environ.get('HOST3').strip())
    hosts.append(os.environ.get('HOST4').strip())
    hosts.append(os.environ.get('HOST5').strip())
    return hosts
    
def setUpModule():
    """
    gpstop a cluster if up and running. Since the tests take care of initsystem themselves and CI sets up a cluster for now,
    we do this at the beginning of this module
    """
    tinctest.logger.info("Setting up test module. gpstop and gpseginstall")
    result = True
    try:
        result = GpStop().run_gpstop_cmd()
    except:
        tinctest.logger.warning("gpstop failed with errors / warnings")
    if not result:
        tinctest.logger.warning("gpstop failed with errors / warnings")
    
    #return
    hosts = get_gpexpand_hosts()
    cmd = GpSegInstall(os.environ.get('GPHOME'), hosts)
    result = cmd.run(validate=True)
    if result.rc > 0:
        tinctest.logger.error("gpseginstall failed with an error code: %s" %result)
        raise GPExpandTestCaseException("gpseginstall failed with an error code. Failing the test module")
    tinctest.logger.info("Finished setting up test module successfully")

@tinctest.skipLoading('model, disable direct loading')
class GPExpandTestCase(MPPTestCase, ScenarioTestCase):
    """
    Test for gpexpand script for HAWQ.  It is a destructive operation in that
    after the test run the cluster is not the same state as before.  Therefore,
    it is recommended to run this test case separately from others.
    """

    def __init__(self, methodName):
        self.gpinitconfig_template = local_path('configs/gpinitconfig_template')
        self.gpinitconfig_mirror_template = local_path('configs/gpinitconfig_mirror_template')
        
        # Output directory specific to this test
        self.testcase_out_dir = os.path.join(self.get_out_dir(), methodName)

        self.testcase_init_file = os.path.join(self.testcase_out_dir, 'gpinitconfig')
        self.testcase_hosts_file = os.path.join(self.testcase_out_dir, 'hosts')
        self.testcase_gpexpand_file = os.path.join(self.testcase_out_dir, 'gpexpand_input')

        self.hosts = ['localhost']

        self.port_base = '20500'
        self.master_port = os.environ.get('PGPORT', '10300')
        self.mirror_port_base = '21500'
        self.rep_port_base = '22500'
        self.mirror_rep_port_base = '23500'

        self.testcase_primary_dir = os.path.join(self.testcase_out_dir, 'data/primary')
        self.testcase_mirror_dir = os.path.join(self.testcase_out_dir, 'data/mirror')
        self.testcase_master_dir = os.path.join(self.testcase_out_dir, 'data/master')
        self.testcase_filespace_dir = os.path.join(self.testcase_out_dir, 'data/filespace')        

        self.expansion_host_list = ''
        
        # Test metadata
        # Whether to do gpinitsystem or not
        self.gpinitsystem = True
        self.mirror_enabled = False
        self.number_of_segments = 2
        self.number_of_hosts = 1
        self.standby_enabled = False

        self.number_of_expansion_hosts = 0
        self.number_of_expansion_segments = 0
        self.number_of_parallel_table_redistributed = 4

        # This flag controls whether or not to run workloads before expansion
        self.run_workload = True
        
        # This flag controls whethere to do a delete system after the test
        self.gpdeletesystem = True

        # This flag controls whether to do a cleanup of expansion schema after the test
        self.cleanup_expansion = True

        # This flag controls whether to do data redistribution after expansion
        self.data_redistribution = True

        self.ranks_enabled = False
        self.duration_enabled = False

        self.use_parallel_expansion = False
        self.use_end_time = False
        self.use_interview = False
        self.use_filespaces = False
        self.use_host_file = False


        super(GPExpandTestCase, self).__init__(methodName)
        

    def _infer_metadata(self):
        super(GPExpandTestCase, self)._infer_metadata()

        try:
            self.gpinitsystem = self._metadata.get('gpinitsystem', "True").lower() in ['true', 'yes']
            self.mirror_enabled = self._metadata.get('mirror_enabled', "False").lower() in ['true', 'yes']
            self.number_of_segments = int(self._metadata.get('number_of_segments', '2'))
            self.number_of_hosts = int(self._metadata.get('number_of_hosts', '1'))
            self.standby_enabled = self._metadata.get('standby_enabled', "False").lower() in ['true', 'yes']
            
            self.gpdeletesystem = self._metadata.get('gpdeletesystem', "True").lower() in ['true', 'yes']

            # Control whether or not to run the workload before expansion
            self.run_workload = self._metadata.get('run_workload', "True").lower() in ['true', 'yes']
            
            # Control whether or not to cleanup the expansion schema
            self.cleanup_expansion = self._metadata.get('cleanup_expansion', "True").lower() in ['true', 'yes']
            self.use_parallel_expansion = self._metadata.get('use_parallel_expansion', "False").lower() in ['true', 'yes']
            self.use_end_time = self._metadata.get('use_end_time', "False").lower() in ['true', 'yes']
            self.use_interview = self._metadata.get('use_interview', "False").lower() in ['true', 'yes']
            self.use_filespaces = self._metadata.get('use_filespaces', "False").lower() in ['true', 'yes']
            self.use_host_file = self._metadata.get('use_host_file', "False").lower() in ['true', 'yes']


            # Control whether or not to run data redistribution as a part of the test
            self.data_redistribution = self._metadata.get('data_redistribution', "True").lower() in ['true', 'yes']

            self.number_of_expansion_hosts = int(self._metadata.get('number_of_expansion_hosts', '0'))
            self.number_of_expansion_segments = int(self._metadata.get('number_of_expansion_segments', '0'))
            self.number_of_parallel_table_redistributed = int(self._metadata.get('number_of_parallel_table_redistributed', '4'))

            self.ranks_enabled = self._metadata.get('ranks_enabled','False').lower() in ['true', 'yes']
            self.duration_enabled = self._metadata.get('duration_enabled','False').lower() in ['true', 'yes']
        except Exception, e:
            tinctest.logger.exception(e)
            tinctest.logger.error("Exception while parsing metadata: %s" %e)
            self.load_fail(GPExpandTestCaseException, e)


    def setUp(self):
        super(GPExpandTestCase, self).setUp()
        # Doing this in setUp to not impact test construction.
        self.hosts = get_gpexpand_hosts()

        mdd = os.path.join(self.testcase_master_dir, 'gpseg-1')
        os.environ["MASTER_DATA_DIRECTORY"] = mdd

        #initial config has master on HOST1 and segments on HOST2 and HOST3.
        #If we choose to add only segments ie self.number_of_expansion_hosts == 0, in the interview process we say use HOST1 and HOST2
        #if we choose to add expansion hosts ,ie self.number_of_expansion_hosts == 2, in the interview process we say use HOST3 and HOST4
        if self.number_of_expansion_hosts == 0:
            self.expansion_host_list = "%s , %s" %(self.hosts[1], self.hosts[2])
        elif self.number_of_expansion_hosts == 2:
            self.expansion_host_list = "%s , %s" %(self.hosts[3], self.hosts[4])
        if self.gpinitsystem:
            self._do_gpinitsystem()
        if self.standby_enabled:
            self._do_gpinitstandby()
        if self.use_filespaces:
            tinctest.logger.info("Setting filespaces")
            gpfs=Gpfilespace()
            gpfs.create_filespace('expand_filespace')

            res = {'rc': 0, 'stdout' : '', 'stderr': ''}
 
            cmdStr="export MASTER_DATA_DIRECTORY=%s; gpfilespace --movetransfilespace expand_filespace" % (mdd)
            run_shell_command(cmdStr, 'create segment dirs', res)
            if res['rc'] > 0:
                raise GpExpandTestCaseException("Failed to movetransfilespace")

            cmdStr="export MASTER_DATA_DIRECTORY=%s; gpfilespace --movetempfilespace expand_filespace" % (mdd)
            run_shell_command(cmdStr, 'create segment dirs', res)
            if res['rc'] > 0:
                raise GpExpandTestCaseException("Failed to movetempfilespace")

        tinctest.logger.info("Performing setup tasks")
        self._setup_gpexpand()


    def _setup_gpexpand(self):
        """
        Takes care of creating all the directories required for expansion
        and generating input files for gpexpand
        """
        # Generate gpexpand config files
        try:
            self._generate_gpexpand_input_files()
        except Exception, e:
            tinctest.logger.exception("Encountered exception during generation of input files. Deleting system: %s " %e)
            result = GpDeleteSystem(mdd=os.path.join(self.testcase_master_dir, 'gpseg-1')).run(validate=False)
            if result.rc > 0:
                tinctest.logger.warning("Failed to delete system for the test case, may already be deleted: %s" %result)
            raise

    def tearDown(self):
        # just stop the cluster if there is a failure, to preserve the data directories for debugging
        if self._resultForDoCleanups.failures or self._resultForDoCleanups.errors:
            result = True
            try:
                result = GpStop().run_gpstop_cmd(mdd=os.path.join(self.testcase_master_dir, 'gpseg-1'))
            except Exception, e:
                tinctest.logger.exception(e)
                tinctest.logger.warning("gpstop failed with errors / warnings")
        else:
            if self.gpdeletesystem:
                result = GpDeleteSystem(mdd=os.path.join(self.testcase_master_dir, 'gpseg-1')).run(validate=False)
                if result.rc > 0:
                    tinctest.logger.warning("Failed to delete system for the test case, may already be deleted: %s" %result)
        super(GPExpandTestCase, self).tearDown()

    def _do_gpinitsystem(self):    
        # Takes care of initializing a cluster according to the test configuration
        TINCSystem.make_dirs(self.testcase_out_dir, force_create=True)
        TINCSystem.make_dirs(self.testcase_master_dir, force_create=True)

        segment_host_file = '/tmp/segment_hosts'
        with open(segment_host_file, 'w') as f:
            for host in self.hosts:
                f.write(host)
                f.write('\n')

        # Create primary dirs
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpssh -f %s -e 'rm -rf %s; mkdir -p %s'" %(segment_host_file, self.testcase_primary_dir, self.testcase_primary_dir), 'create segment dirs', res)
        if res['rc'] > 0:
            raise GpExpandTestCaseException("Failed to create segment directories")

        # Create mirror dirs
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpssh -f %s -e 'rm -rf %s; mkdir -p %s'" %(segment_host_file, self.testcase_mirror_dir, self.testcase_mirror_dir), 'create segment dirs', res)
        if res['rc'] > 0:
            raise GpExpandTestCaseException("Failed to create segment directories")

        # Create filespace dirs
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpssh -f %s -e 'rm -rf %s; mkdir -p %s'" %(segment_host_file, self.testcase_filespace_dir, self.testcase_filespace_dir), 'create segment dirs', res)
        if res['rc'] > 0:
            raise GpExpandTestCaseException("Failed to create segment directories")

        # Generate the config files to initialize the cluster
        self._generate_gpinit_config_files()
        self.assertTrue(os.path.exists(self.testcase_init_file))
        self.assertTrue(os.path.exists(self.testcase_hosts_file))

        # TODO: Do some clean up before init
        # Init the system
        # Still have to figure out if it is a good idea to init for every test or not
        # Pro: easier to develop self contained tests Con: time taken for every test
        result = GpInitSystem(self.testcase_init_file).run(validate=False)

        # initsystem returns 1 for warnings and 2 for errors
        if result.rc > 1:
            tinctest.logger.error("Failed initializing the cluster: %s" %result)
            raise GPExpandTestCaseException("Failed initializing the cluster. Look into gpAdminLogs for more information")

    def _do_gpinitstandby(self):
        """
        Initializes a standby host in the second host in the list self.hosts.
        setUpModule would have taken care of installing the binaries on all the hosts.
        Hence all we have to do here is a gpinitstandby
        """
        standby_host = self.hosts[1]
        tinctest.logger.info("Initializing standby master on host: %s" %standby_host)

        # Create master directory on the standby host
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpssh -h %s -e 'rm -rf %s; mkdir -p %s'" %(standby_host, self.testcase_master_dir, self.testcase_master_dir), 'create master dir on standby host', res)
        if res['rc'] > 0:
            raise GpExpandTestCaseException("Failed to create segment directories")

        # Do gpinitstandby
        cmd = GpInitStandby(standby_host, mdd=os.path.join(self.testcase_master_dir, 'gpseg-1'))
        result = cmd.run(validate=True)
        if result.rc > 0:
            tinctest.logger.error("gpinitstandby failed with an error code: %s" %result)
            raise GPExpandTestCaseException("gpinitstandby failed with an error code. Failing the test module")
        gpsc = PSQL.run_sql_command("SELECT * FROM gp_segment_configuration")
        tinctest.logger.info("Segment configuration: %s" %gpsc)
        
        
    def _generate_gpinit_config_files(self):
        transforms = {'%PORT_BASE%': self.port_base,
                      '%MASTER_HOST%': self.hosts[0], # First item in self.hosts
                      '%HOSTFILE%': self.testcase_hosts_file,
                      '%MASTER_PORT%': self.master_port,
                      '%MASTER_DATA_DIR%': self.testcase_master_dir,
                      '%DATA_DIR%': (self.testcase_primary_dir + ' ') * self.number_of_segments
                      }

        if self.mirror_enabled:
            transforms['%MIRROR_DIR%'] = (self.testcase_mirror_dir + ' ') * self.number_of_segments
            transforms['%MIRROR_PORT_BASE%'] = self.mirror_port_base
            transforms['%REP_PORT_BASE%'] = self.rep_port_base
            transforms['%MIRROR_REP_PORT_BASE%'] = self.mirror_rep_port_base

        # First generate host file based on number_of_hosts
        with open(self.testcase_hosts_file, 'w') as f:
            for host in self.hosts[1:self.number_of_hosts+1]:
                f.write(host + '\n')

        if self.mirror_enabled:
            TINCSystem.substitute_strings_in_file(self.gpinitconfig_mirror_template,
                                                  self.testcase_init_file,
                                                  transforms)
        else:
            TINCSystem.substitute_strings_in_file(self.gpinitconfig_template,
                                                  self.testcase_init_file,
                                                  transforms)

    def _generate_gpexpand_input_files(self):
        config = GPDBConfig()

        # use the last segment as a template
        # TODO: This logic needs fixing when we enable mirror tests
        seg = (sorted(config.record, lambda a, b: b.content - a.content)).pop(0)
        # Find max db id for generating gpexpand input file
        max_db_id = max([r.dbid for r in config.record])
        self.assertNotEqual(seg.content, -1)


        def next_datadir(datadir, i=1):
            # /path/to/foo/seg12 -> /path/to/foo/seg13
            lefthand = datadir.rstrip('0123456789')
            segnum = int(datadir[len(lefthand):])
            return lefthand + str(segnum + i)

        with open(self.testcase_gpexpand_file, 'w') as f:
            # For existing hosts, add self.number_of_expansion_segments
            # For new hosts add existing number of segments + number_of_expansion_segments

            # Existing hosts
            existing_hosts = config.get_hosts(segments=True)
            existing_number_of_segments = 0

            cnt = 1

            for host in existing_hosts:
                # Find the existing number of segments in the existing hosts
                existing_number_of_segments = config.get_segments_count_per_host()[host]

                for i in range(self.number_of_expansion_segments):
                    f.write(
                    "{host}:{addr}:{port}:{datadir}:{dbid}:{content}:{role}\n".format(
                        host=host,
                        addr=host,
                        port= int(seg.port) + cnt,
                        datadir=next_datadir(seg.datadir, cnt),
                        dbid=int(max_db_id) + cnt,
                        content= int(seg.content) + cnt,
                        role='p'
                        ))
                    cnt += 1

            # New hosts
            if self.number_of_expansion_hosts > 0:
                new_expansion_hosts = list(set(self.hosts) - existing_hosts)
                if not new_expansion_hosts:
                    raise GPExpandTestCaseException("No new hosts available for expansion based on the environment variable GPEXPAND_HOSTS: %s" %os.environ.get("GPEXPAND_HOSTS"))

                for host in new_expansion_hosts[0:self.number_of_expansion_hosts]:
                    for i in range(existing_number_of_segments + self.number_of_expansion_segments):
                        f.write(
                            "{host}:{addr}:{port}:{datadir}:{dbid}:{content}:{role}\n".format(
                            host=host,
                            addr=host,
                            port= int(seg.port) + cnt,
                            datadir=next_datadir(seg.datadir, cnt),
                            dbid=int(seg.dbid) + cnt,
                            content= int(seg.content) + cnt,
                            role='p'
                            ))
                        cnt += 1

    def construct_expansion_scenario(self):

        classlist = []
        classlist.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_EnhancedTableFunctionTest.EnhancedTableFunctionTest')
        classlist.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_OSSpecificSQLsTest.OSSpecificSQLsTest')
        classlist.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_AllSQLsTest.AllSQLsTest')
        classlist.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_GPFilespaceTablespaceTest.GPFilespaceTablespaceTest')

        if self.run_workload:
            # Run expansion workload
            if self.use_parallel_expansion:
                self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.PreExpansionWorkloadTests.test_create_parallel_expansion_workload' %self.package_name])
            elif self.duration_enabled or self.use_end_time:
                self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.PreExpansionWorkloadTests.test_create_duration_workload' %self.package_name])
            elif self.ranks_enabled:
                self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.PreExpansionWorkloadTests.test_create_base_workload' %self.package_name])
            else:
                self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.PreExpansionWorkloadTests.test_create_base_workload' %self.package_name])
                self.test_case_scenario.append(classlist , serial=True)

        # Snapshot the distribution policies of all the tables, this will generate the ans file that will be used in TableDistributionPolicyVerify below
        self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.TableDistrubtionPolicySnapshot' %self.package_name], serial=True)

        # Run interview if enabled
        if self.use_interview:
            self.test_case_scenario.append([('%s.scenarios.run_gpexpand.GpExpandTests.interview' %self.package_name,
                                        {"mdd": os.path.join(self.testcase_master_dir, 'gpseg-1'), # TODO: Be smarter about finding mdd
                                         "primary_data_dir": self.testcase_primary_dir,
                                         "mirror_data_dir": self.testcase_mirror_dir,
                                         "new_hosts": self.expansion_host_list,
                                         "use_host_file": self.use_host_file,
                                         "num_new_segs": self.number_of_expansion_segments,
                                         "filespace_data_dir": self.testcase_filespace_dir,
                                         "mapfile": self.testcase_gpexpand_file})], serial=True)

        # create directories needed for expansion with filespaces
        self.test_case_scenario.append([('%s.scenarios.run_gpexpand.GpExpandTests.create_filespace_dirs' %self.package_name,
                                        {"primary_data_dir": self.testcase_primary_dir,
                                         "mirror_data_dir": self.testcase_mirror_dir,
                                         "filespace_data_dir": self.testcase_filespace_dir})], serial=True)

        # Run expansion
        self.test_case_scenario.append([('%s.scenarios.run_gpexpand.GpExpandTests.run_expansion' %self.package_name,
                                        {"mapfile": self.testcase_gpexpand_file,
                                         "dbname": os.environ.get('PGDATABASE'),
                                         "interview": self.use_interview,
                                         "mdd": os.path.join(self.testcase_master_dir, 'gpseg-1'), # TODO: Be smarter about finding mdd 
                                         "output_dir": self.testcase_out_dir,
                                         "validate": True})], serial=True)

        if self.ranks_enabled:
            self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.SetRanks.test_set_ranks' %self.package_name])


        # Run data redistribution
        if self.data_redistribution:
            if self.duration_enabled or self.use_end_time:    
                self.test_case_scenario.append([('%s.scenarios.run_gpexpand.GpExpandTests.run_redistribution_with_duration' %self.package_name,
                                             {"mdd": os.path.join(self.testcase_master_dir, 'gpseg-1'), # TODO: Same as above
                                              "dbname": os.environ.get('PGDATABASE'),
                                              "output_dir": self.testcase_out_dir,
                                              "use_end_time": self.use_end_time,
                                              "validate": True})], serial=True)
            elif self.use_parallel_expansion:    
                run_tests_in_parallel = []
                run_tests_in_parallel.append(('%s.scenarios.run_gpexpand.GpExpandTests.run_redistribution' %self.package_name,
                                             {"mdd": os.path.join(self.testcase_master_dir, 'gpseg-1'), # TODO: Same as above
                                              "use_parallel_expansion": self.use_parallel_expansion,
                                              "number_of_parallel_table_redistributed": self.number_of_parallel_table_redistributed, 
                                              "dbname": os.environ.get('PGDATABASE'),
                                              "output_dir": self.testcase_out_dir,
                                              "validate": True}))
                run_tests_in_parallel.append(('%s.scenarios.run_gpexpand.GpExpandTests.check_number_of_parallel_tables_expanded' %self.package_name,
                                             {"number_of_parallel_table_redistributed": self.number_of_parallel_table_redistributed}))
                self.test_case_scenario.append(run_tests_in_parallel)
            else:
                self.test_case_scenario.append([('%s.scenarios.run_gpexpand.GpExpandTests.run_redistribution' %self.package_name,
                                             {"mdd": os.path.join(self.testcase_master_dir, 'gpseg-1'), # TODO: Same as above
                                              "dbname": os.environ.get('PGDATABASE'),
                                              "output_dir": self.testcase_out_dir,
                                              "validate": True})], serial=True)

        self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.TableDistributionPolicyVerify' %self.package_name])


        if self.ranks_enabled:
            self.test_case_scenario.append(['%s.scenarios.workloads.test_run_workload.SetRanks.test_verify_ranks' %self.package_name])

        if self.cleanup_expansion:
            self.test_case_scenario.append([('%s.scenarios.run_gpexpand.GpExpandTests.cleanup_expansion' %self.package_name,
                                             {"dbname": os.environ.get('PGDATABASE'),
                                              "output_dir": self.testcase_out_dir,
                                              "mdd": os.path.join(self.testcase_master_dir, 'gpseg-1')})], serial=True)

        self.test_case_scenario.append(['%s.scenarios.run_gpexpand.GpExpandTests.mirror_and_catalog_validation' %self.package_name], serial=True)


class GpExpandTests(GPExpandTestCase):
    """
        Regarding tags below, the "part1" and "part2" tags below split the test cases
        into two groups based on average test duration,
        The two groups run in parallel to reduce the overall runtime of the suite.
        If you add additional tests, attempt to keep the groups relatively even in total duration.
        Always add a tag for either "part1" or "part2" so that the test will be targeted by the
        pulse "build stage" property override for variable "BLDWRAP_TINC_ARGS" found in
        http://pulse-cloud.gopivotal.com/admin/projects/GPDB-gpexpand-parallel/
    """

    def test_expand_no_segments_two_hosts(self):
        """
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_hosts 2
        @mirror_enabled true
        @use_interview true
        @tags part1
        """
        self.construct_expansion_scenario()

    def test_expand_one_segments_no_hosts(self):
        """
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_segments 1
        @mirror_enabled true
        @use_interview true
        @tags part1
        """
        self.construct_expansion_scenario()

    def test_expand_one_segments_two_hosts(self):
        """ 
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_hosts 2
        @number_of_expansion_segments 1
        @mirror_enabled true
        @use_interview true
        @tags part1
        """
        self.construct_expansion_scenario()

    def test_expand_transtempfs_parallel(self):
        """ 
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_segments 1
        @mirror_enabled true
        @use_interview true
        @use_parallel_expansion true
        @number_of_parallel_table_redistributed 4
        @use_filespaces true
        @tags part2
        """
        self.construct_expansion_scenario()

    def test_expand_with_ranks(self):
        """ 
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_hosts 2
        @number_of_expansion_segments 0
        @mirror_enabled true
        @use_interview true
        @ranks_enabled true
        @use_host_file true
        @tags part2
        """
        self.construct_expansion_scenario()

    def test_expand_with_duration(self):
        """ 
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_segments 1
        @mirror_enabled true
        @use_interview true
        @duration_enabled true
        @tags part2
        """
        self.construct_expansion_scenario()

    def test_expand_with_endtime_and_standby(self):
        """ 
        @number_of_segments 1
        @number_of_hosts 2
        @number_of_expansion_segments 1
        @mirror_enabled true
        @use_interview true
        @use_end_time true
        @standby_enabled True
        @tags part2
        """
        self.construct_expansion_scenario()






