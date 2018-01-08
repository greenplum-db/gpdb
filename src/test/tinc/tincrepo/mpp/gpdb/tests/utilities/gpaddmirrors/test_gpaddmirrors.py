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

import sys
import subprocess
from sets import Set
import pexpect
import time
import os

import tinctest
import unittest2 as unittest

from mpp.lib.config import GPDBConfig
from mpp.models import MPPTestCase

from tinctest.models.scenario import ScenarioTestCase
from tinctest.lib.system import TINCSystem
from tinctest.lib import local_path, run_shell_command
from tinctest.lib import Gpdiff
from tinctest.main import TINCException

from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from gppylib.commands.base import Command, REMOTE
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.lib.PSQL import PSQL

class GpInitSystem(Command):
    """This is a wrapper for gpinitsystem."""
    def __init__(self, config_file):
        cmd_str = 'gpinitsystem -a -c  %s' % (config_file)
        Command.__init__(self, 'run gpinitsystem', cmd_str)

    def run(self, validate=True):
        tinctest.logger.info("Running initialization: %s" % self)
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
        tinctest.logger.info("Running gpinitstandby: %s" % self)
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
                                    
class GPAddmirrorsTestCaseException(TINCException):
    pass


class GPAddmirrorsTestCase(MPPTestCase):

    def __init__(self, methodName):
        self.config = GPDBConfig()
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.seg_prefix = os.path.basename(self.mdd).split('-')[0]
        self.master_host = self.config.get_masterhost()
        self.gpinitconfig_template = local_path('configs/gpinitconfig_template')
        self.datadir_config_file = local_path('configs/datadir_config_file') 
        self.mirror_config_file = local_path('configs/mirror_config_file')
        self.gpinitconfig_file = local_path('configs/gpinitconfig')
        self.host_file = local_path('configs/hosts')
        self.hosts = self.config.get_hosts(segments = True)

        self.port_base = '40000'
        self.master_port = os.environ.get('PGPORT', '5432')
        self.primary_data_dir = self.config.get_host_and_datadir_of_segment(dbid = 2)[1]
        # initially set the mirror data dir same to primary's
        self.mirror_data_dir = os.path.join(os.path.dirname(os.path.dirname(self.primary_data_dir)), 'mirror')
        self.gpinitsystem = True
        self.number_of_segments = self.config.get_countprimarysegments()
        self.number_of_segments_per_host = self.number_of_segments / len(self.hosts)
        self.standby_enabled = False
        self.number_of_parallelism = 4
        self.fs_location = []

        super(GPAddmirrorsTestCase, self).__init__(methodName)

    def setUp(self):
        super(GPAddmirrorsTestCase, self).setUp()

    def _setup_gpaddmirrors(self, port_offset=1000):
        """
        Takes care of creating all the directories required for gpaddmirrors
        and generating input files for gpaddmirrors
        """
        # Generate gpaddmirrors config files
        try:
            self._generate_gpaddmirrors_input_files(port_offset)
        except Exception, e:
            tinctest.logger.exception("Encountered exception during generation of input files: %s" % e)
            raise

    def tearDown(self):
        super(GPAddmirrorsTestCase, self).tearDown()


    def check_mirror_seg(self, master=False):
        tinctest.logger.info("running check mirror")
        dbstate = DbStateClass('run_validation')
        dbstate.check_mirrorintegrity(master=master)


    def _generate_gpinit_config_files(self):
        transforms = {'%SEG_PREFIX%': self.seg_prefix,
                      '%PORT_BASE%': self.port_base,
                      '%MASTER_HOST%': self.master_host, # First item in self.hosts
                      '%HOSTFILE%': self.host_file,
                      '%MASTER_PORT%': self.master_port,
                      '%MASTER_DATA_DIR%': os.path.dirname(self.mdd),
                      '%DATA_DIR%': (os.path.dirname(self.primary_data_dir) + ' ') * self.number_of_segments_per_host 
                      }

        # First generate host file based on number_of_hosts
        with open(self.host_file, 'w') as f:
            for host in self.hosts:
                f.write(host + '\n')
        TINCSystem.substitute_strings_in_file(self.gpinitconfig_template,
                                              self.gpinitconfig_file,
                                              transforms)


    def format_sql_result(self, sql_command=None):
        if sql_command is None:
            tinctest.logger.warning("Please provide a sql command")
            return None        
        result = PSQL.run_sql_command(sql_command, flags='-q -t', dbname='template1')
        result = result.strip()
        rows = result.split('\n')
        formatted_rows = []
        for row in rows:
            cols = row.split('|')
            cols = [col.strip() for col in cols]
            formatted_rows.append(cols)
        return formatted_rows

    def _do_gpdeletesystem(self):
        result = GpDeleteSystem(mdd=self.mdd).run(validate=False)
        if result.rc > 0:
            tinctest.logger.warning("Failed to delete system for the test case, may already be deleted: %s" %result)

    def _do_gpinitsystem(self):    
        # Check the config files to initialize the cluster
        self.assertTrue(os.path.exists(self.gpinitconfig_file))
        self.assertTrue(os.path.exists(self.host_file))

        # cleanup data directories before running gpinitsystem
        self._cleanup_segment_data_dir(self.host_file, os.path.dirname(self.primary_data_dir))
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("rm -rf %s; mkdir -p %s" % (os.path.dirname(self.mdd), os.path.dirname(self.mdd)), 'create master dir', res)
        if res['rc'] > 0:
            raise GPAddmirrorsTestCaseException("Failed to create master directories")

        result = GpInitSystem(self.gpinitconfig_file).run(validate=False)

        # initsystem returns 1 for warnings and 2 for errors
        if result.rc > 1:
            tinctest.logger.error("Failed initializing the cluster: %s" % result)
            raise GPAddmirrorsTestCaseException("Failed initializing the cluster. Look into gpAdminLogs for more information")


    def _cleanup_segment_data_dir(self, host_file, segment_data_dir):
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpssh -f %s -e 'rm -rf %s; mkdir -p %s'" %(host_file, segment_data_dir, segment_data_dir), 'create segment dirs', res)
        if res['rc'] > 0:
            raise GPAddmirrorsTestCaseException("Failed to create segment directories")        

    def _do_gpinitstandby(self):
        """
        Initializes a standby host on a host which is different from master host.
        """
        for host in self.hosts:
            if host != self.master_host:
                 standby_host = host
                 break
        tinctest.logger.info("Initializing standby master on host: %s" % standby_host)

        # Create master directory on the standby host
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpssh -h %s -e 'rm -rf %s; mkdir -p %s'" %(standby_host, os.path.dirname(self.mdd), os.path.dirname(self.mdd)), 'create master dir on standby host', res)
        if res['rc'] > 0:
            raise GPAddmirrorsTestCaseException("Failed to create segment directories")

        # Do gpinitstandby
        cmd = GpInitStandby(standby_host, mdd=self.mdd)
        result = cmd.run(validate=False)
        if result.rc > 0:
            tinctest.logger.error("gpinitstandby failed with an error code: %s" %result)
            raise GPAddmirrorsTestCaseException("gpinitstandby failed with an error code. Failing the test module")

    def _generate_gpaddmirrors_input_files(self, port_offset=1000):
        with open(self.datadir_config_file, 'w') as f:
            for i in range (0, self.number_of_segments_per_host):
                f.write(self.mirror_data_dir+'\n')
        if port_offset != 1000:
            cmdStr = 'gpaddmirrors -p %s -o %s -m %s -d %s' % (port_offset, self.mirror_config_file, self.datadir_config_file, self.mdd)
        else:
            cmdStr = 'gpaddmirrors -o %s -m %s -d %s' % (self.mirror_config_file, self.datadir_config_file, self.mdd)
        Command('generate the sample_mirror_config file', cmdStr).run(validateAfter=True)


    def verify_config_file_with_gp_config(self):
        """
        compares the gp_segment_configuration with input file mirror_data_dir, double check
        if the cluster is configured as intended
        """
        with open(self.mirror_config_file, 'r') as f:
            next(f)            
            for line in f:
                line = line.strip()
                mirror_seg_infor = line.split('=')[1]
                cols = mirror_seg_infor.split(':')
                content_id = cols[0]
                adress = cols[1]
                port = cols[2]
                query_on_configuration = '''select * from gp_segment_configuration where content=\'%s\' and address=\'%s\'
                                            and port=\'%s\'''' % (content_id, adress, port)
                config_info = PSQL.run_sql_command(query_on_configuration, flags='-q -t', dbname='template1')
                config_info = config_info.strip()
                # as intended, the entry should be existing in the cluster
                self.assertNotEqual(0, len(config_info))

    def run_simple_ddl_dml(self):
        """
        Run simple ddl and dml statement, to verify that cluster is functioning properly
        """
        setup_sql = 'DROP TABLE IF EXISTS gpaddmirrors_table; CREATE TABLE gpaddmirrors_table(KEY INTEGER, CONTENT VARCHAR(199));'
        insert_sql = 'INSERT INTO gpaddmirrors_table VALUES( generate_series(1,500), \'This is a simple test for addmirrors\' );'
        verify_sql = 'SELECT COUNT(*) FROM gpaddmirrors_table;'
        PSQL.run_sql_command(setup_sql, flags='-q -t', dbname='template1')
        PSQL.run_sql_command(insert_sql, flags='-q -t', dbname='template1')
        result = PSQL.run_sql_command(verify_sql, flags='-q -t', dbname='template1')
        result = result.strip()
        self.assertEqual(500, int(result))


class GpAddmirrorsTests(GPAddmirrorsTestCase):
    """
    @description gpaddmirrors test suite
    @tags gpaddmirrors
    """

    def setUp(self):
        super(GpAddmirrorsTests, self).setUp()
        if self.config.has_mirror():
            self._generate_gpinit_config_files()
            self._do_gpdeletesystem()
            self._do_gpinitsystem()
            time.sleep(5)

    def tearDown(self):
        super(GpAddmirrorsTests, self).tearDown()


# The following test should not be ported because the gpaddmirror functionality is already tested by other tests, and
# this test in particular is only testing if worker pool can handle a batch size of 4.

    def test_batch_size_4(self):
        """
        check the batch size option -B of gpaddmirrors, depending on how many mirror segment to setup, otherwise, it will start up to 10
        """
        gprecover = GpRecover()
        self._setup_gpaddmirrors()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)

        workers = Set()
        batch_size = 4
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpaddmirrors -a -i %s -B %s -d %s --verbose" % (self.mirror_config_file, batch_size, self.mdd), 'run gpaddmirrros batch size %s' % batch_size, res)
        self.assertEqual(0, res['rc'])
        lines = res['stdout'].split('\n')
        for line in lines:
            if 'worker' in line and 'haltWork' in line:
                elems = line.split(' ')[1]
                worker = elems.split('-')[-1]
                workers.add(worker)
        self.assertEquals(len(workers), batch_size)            
        gprecover.wait_till_insync_transition()
        self.verify_config_file_with_gp_config()
        self.check_mirror_seg()


#The following tests need to be ported to Behave.

    def test_mirror_spread(self):
        """
        Mirror spreading will place each mirror on a different host within the Greenplum  Database array
        """
        gprecover = GpRecover()
        if self.number_of_segments_per_host > len(self.hosts):
            self.skipTest('skipping test since the number of host is less than number of segments per hosts')
        self._setup_gpaddmirrors()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpaddmirrors -a -i %s -s -d %s --verbose" % (self.mirror_config_file, self.mdd), 'run gpaddmirrros with mirror spreading', res)
        self.assertEqual(0, res['rc'])
        check_mirror_spreading = '''SELECT A.hostname, B.hostname 
                                  FROM gp_segment_configuration A, gp_segment_configuration B 
                                  WHERE A.preferred_role = \'p\' AND B.preferred_role = \'m\' AND A.content = B.content AND A.hostname <> B.hostname;'''
        result = PSQL.run_sql_command(check_mirror_spreading, flags='-q -t', dbname='template1')
        result = result.strip()
        self.assertNotEqual(0, len(result))
        rows = result.split('\n')
        self.assertEqual(self.number_of_segments, len(rows))
        gprecover.wait_till_insync_transition()
        self.verify_config_file_with_gp_config()
        self.check_mirror_seg()

    def test_with_standby(self):
        """
        check that cluster's host address is same when it is with standby and without standby
        """
        if not self.config.is_multinode():
            self.skipTest('skipping test since the cluster is not multinode')

        gprecover = GpRecover()

        self._setup_gpaddmirrors()
        # adding mirrors first
        self._setup_gpaddmirrors()
        self._generate_gpinit_config_files()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpaddmirrors -a -i %s -s -d %s --verbose" % (self.mirror_config_file, self.mdd), 'run gpaddmirrros with mirror spreading', res)
        self.assertEqual(0, res['rc'])
        gprecover.wait_till_insync_transition()
        
        get_mirror_address = 'SELECT content, address FROM gp_segment_configuration WHERE preferred_role = \'m\';'
        rows = self.format_sql_result(get_mirror_address)
        # create a dictionary for mirror and its host address
        mirror_hosts_wo_stdby = {}
        for row in rows:
            content = row[0]
            address = row[1]
            mirror_hosts_wo_stdby[content] = address

        # delete and reinitialize cluster again
        self._do_gpdeletesystem()
        self._do_gpinitsystem()
        gprecover.wait_till_insync_transition()
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        # create standby, needs to get a new config_info instance for new cluster
        config_info = GPDBConfig()
        if not config_info.has_master_mirror():
            self._do_gpinitstandby()

        self._setup_gpaddmirrors()
        self._generate_gpinit_config_files()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)

        # add mirror for the new cluster which has standby configured
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpaddmirrors -a -i %s -s -d %s --verbose" % (self.mirror_config_file, self.mdd), 'run gpaddmirrros with mirror spreading', res)
        self.assertEqual(0, res['rc'])
        gprecover.wait_till_insync_transition()
        # verify that the configuration will be same as mirror_config_file specified
        self.verify_config_file_with_gp_config()
        self.check_mirror_seg()

        rows = self.format_sql_result(get_mirror_address)
        mirror_hosts_with_stdby = {}
        for row in rows:
            content = row[0]
            address = row[1]
            mirror_hosts_with_stdby[content] = address
        for key in mirror_hosts_wo_stdby:
            self.assertEqual(mirror_hosts_wo_stdby[key], mirror_hosts_with_stdby[key])

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpinitstandby -ar", 'remove standby', res)
        if res['rc'] > 0:
           raise GPAddmirrorsTestCaseException("Failed to remove the standby")

    def test_with_fault_injection(self):
        """
        add new mirrors run workload to verify if cluster functioning correctly, and 
        inject the mirror to bring cluster into change tracking, then recoverseg
        """
        filerepUtil = Filerepe2e_Util()
        gprecover = GpRecover()
        self._setup_gpaddmirrors()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpaddmirrors -a -i %s -d %s --verbose" % (self.mirror_config_file, self.mdd), 'run gpaddmirrros with fault injection', res)
        gprecover.wait_till_insync_transition()
        self.assertEqual(0, res['rc'])
        self.run_simple_ddl_dml()

        # after adding new mirrors, check the intergrity between primary and mirror
        self.check_mirror_seg()
        out_file = local_path('inject_fault_into_ct')
        filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='mirror', H='ALL', outfile=out_file)
        # trigger the transtion to change tracking
        PSQL.run_sql_command('drop table if exists foo;', dbname = 'template1')
        filerepUtil.wait_till_change_tracking_transition()
        gprecover.incremental()
        gprecover.wait_till_insync_transition()
        out_file=local_path('reset_fault')
        filerepUtil.inject_fault(f='filerep_consumer', m='async', y='reset', r='mirror', H='ALL', outfile=out_file)

    def test_with_concurrent_workload(self):
        """
        add new mirrors while concurrent workload in progress, check that mirrors added
        and current workload won't get affected, in the end, run checkmirrorseg.
        Note that: adding mirrors while running workload has checkmirrorseg issue with MPP-24311
        """
        gprecover = GpRecover()
        self._setup_gpaddmirrors()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)
        sql_setup_file = local_path('sql/ao_heap_table_setup.sql') 
        sql_file = local_path('sql/ao_heap_table.sql')
        pg_stat_activity = 'SELECT * FROM pg_stat_activity;'
        PSQL.run_sql_file(sql_setup_file)
        subprocess.Popen(["psql", "-f", sql_file])
        time.sleep(15)
        subprocess.Popen(["gpaddmirrors", "-ai", self.mirror_config_file, "-d", self.mdd])
        time.sleep(15)
        result = PSQL.run_sql_command(pg_stat_activity, flags='-q -t', dbname='template1')
        result = result.strip()
        rows = result.split('\n')
        self.assertTrue(len(rows) > 1)
        while len(rows) > 1:
            result = PSQL.run_sql_command(pg_stat_activity, flags='-q -t', dbname='template1')
            result = result.strip()
            rows = result.split('\n')
            time.sleep(3)
        gprecover.wait_till_insync_transition()
        self.verify_config_file_with_gp_config()
        # ignore check_mirror_seg due to MPP-24311
        #self.check_mirror_seg()


    def test_gpaddmirrors_with_workload(self):
        """
        add new mirrors after creating some workload in progress, check that mirrors added
        and checkmirrorseg passes.
        """
        gprecover = GpRecover()
        self._setup_gpaddmirrors()
        self._cleanup_segment_data_dir(self.host_file, self.mirror_data_dir)
        sql_setup_file = local_path('sql/ao_heap_table_setup.sql')
        sql_file = local_path('sql/ao_heap_table.sql')
        pg_stat_activity = 'SELECT * FROM pg_stat_activity;'
        PSQL.run_sql_file(sql_setup_file)
        PSQL.run_sql_file(sql_file)
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("gpaddmirrors -a -i %s -d %s --verbose" % (self.mirror_config_file, self.mdd), 'run gpaddmirrros with fault injection', res)
        self.assertEqual(0, res['rc'])
        gprecover.wait_till_insync_transition()
        self.verify_config_file_with_gp_config()
        self.check_mirror_seg()


    def test_interview(self):
        gprecover = GpRecover()
        child = pexpect.spawn('gpaddmirrors')
        #child.logfile = sys.stdout
        for i in range(0, self.number_of_segments_per_host):
            child.expect('Enter mirror segment data directory location.*.\r\n')        
            child.sendline(self.mirror_data_dir)
        child.expect('Continue with add mirrors procedure Yy|Nn (default=N):')
        child.sendline('Y')
        child.expect(pexpect.EOF)
        # wait until cluste totally synced, then run gpcheckmirrorseg
        gprecover.wait_till_insync_transition()
        self.check_mirror_seg()
        self._do_gpdeletesystem()
        self._do_gpinitsystem()
