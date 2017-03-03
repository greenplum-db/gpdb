"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

import os
from datetime import date
import unittest2 as unittest
import tinctest
import pexpect
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.utilities.backup_restore.incremental import BackupTestCase, is_earlier_than
from mpp.gpdb.tests.utilities.backup_restore import read_config_yaml
from tinctest.lib import local_path
from mpp.lib.gpdb_util import GPDBUtil

'''
@Incremental Backup
'''
gpdb_util = GPDBUtil()
version = gpdb_util.getGPDBVersion()

# alternative storage unit is used for dynamic storage unit option, must be created(manually currently) first
alternative_storage_unit = 'TEMP'

# alternative storage unit is used for dynamic storage unit option, must be created(manually currently) first
alternative_storage_unit = 'TEMP'

class test_backup_restore(BackupTestCase):
    """

    @description test cases for incremental backup with ddboost
    @created 2014-06-10 10:10:10
    @modified 2014-06-20 10:10:15
    @tags backup restore gpcrondump gpdbrestore incremental ddboost
    @product_version gpdb: [4.3.2.x- main]
    @gucs gp_create_table_random_default_distribution=off
    """

    def __init__(self, methodName):
        super(test_backup_restore, self).__init__(methodName)
        ddboost_path_yml = os.path.join(os.getenv('MASTER_DATA_DIRECTORY'),
                                        'ddboost_config.yml')
        tinctest.logger.info("reading ddboost config yml")
        ddboost_config = read_config_yaml(ddboost_path_yml)
        self.HOST = ddboost_config['DDBOOST_HOST']
        self.USER = ddboost_config['DDBOOST_USER']
        self.PASSWORD = ddboost_config['DDBOOST_PASSWORD']
        self.BACKUPDIR = BackupTestCase.TSTINFO['DDBOOST_DIR']
        self.create_filespace()
        self.cleanup_backup_files()

    def cleanup(self):
        self.cleanup_backup_files(location = self.BACKUPDIR)
        path = '%s/%s/' % (os.environ.get('MASTER_DATA_DIRECTORY'), self.BACKUPDIR)
        self.cleanup_backup_files(location = path)

    def ddboost_config_setup(self, backup_dir=None, storage_unit=None):
        cmd_remove_config = "gpcrondump --ddboost-config-remove"
        (err, out) = self.run_command(cmd_remove_config)
        if err:
            msg = "Failed to remove previous DD configuration\n"
            raise Exception('Error: %s\n%s'% (msg,out))

        cmd_config = "gpcrondump --ddboost-host %s --ddboost-user %s" % (self.HOST, self.USER)
        cmd_config += " --ddboost-backupdir %s" % (self.BACKUPDIR if backup_dir is None else backup_dir)

        if storage_unit:
            cmd_config += " --ddboost-storage-unit %s" % storage_unit

        cmd_config
        local = pexpect.spawn(cmd_config)
        local.expect('Password: ')
        local.sendline(self.PASSWORD)
        local.expect(pexpect.EOF)
        local.close()

    def test_00_ddboost_config_without_storage_unit(self):
        self.ddboost_config_setup(self.BACKUPDIR)

    def test_00_ddboost_config_with_storage_unit(self):
        self.ddboost_config_setup(self.BACKUPDIR, storage_unit="GPDB1")

    def test_01_incremental_nooptions(self):
        tinctest.logger.info("Test1: gpcromndump --incremental with no options...")

        self.run_workload("backup_dir", 'bkdb1')

        self.run_full_backup(dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb1')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb1')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb1')
        self.run_incr_backup("dirty_dir_3", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_4", 'bkdb1')
        self.get_data_to_file('bkdb1', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_4", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb1', option = '--ddboost', location=self.BACKUPDIR)
        self.validate_restore("restore_dir", 'bkdb1')
        self.compare_table_data('bkdb1')

        self.compare_ao_tuple_count('bkdb1')

    def test_02_incremental_option1(self):
        tinctest.logger.info("Test2: Test for verifying incremental backup  with options -b, -B, -d, -f, -j, -k ; Restore with -s")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb2')
        self.run_full_backup(dbname = 'bkdb2', option = "--ddboost", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb2')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb2', option = "-b, -j, -k, -z --ddboost", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb2')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb2', option = "--ddboost -B 100", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb2')
        self.run_incr_backup("dirty_dir_3", dbname = 'bkdb2', option = "--ddboost -d %s" % os.environ.get('MASTER_DATA_DIRECTORY'), location=self.BACKUPDIR)

        self.run_workload("dirty_dir_4", 'bkdb2')
        self.get_data_to_file('bkdb2', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_4", dbname = 'bkdb2', option = "--ddboost", location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb2', option = '-s bkdb2 -e --ddboost', location=self.BACKUPDIR)
        self.validate_restore("restore_dir", 'bkdb2')
        self.compare_table_data('bkdb2')
        self.compare_ao_tuple_count('bkdb2')

    @unittest.skipIf((os.uname()[0] == 'SunOS'), 'Skipped on Solaris')
    def test_03_incremental_option2(self):

        tinctest.logger.info("Test3: incremental backup  with options -c, --resyncable, -l,--no-owner, --no-privileges, --inserts etc ...")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb3')
        self.run_full_backup(dbname = 'bkdb3', option="--ddboost", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb3')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb3', option = "--ddboost --rsyncable, --no-privileges", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb3')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb3', option = "--ddboost -l new_log_file", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb3')
        self.run_incr_backup("dirty_dir_3", dbname = 'bkdb3', option = "--ddboost --use-set-session-authorization", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_4", 'bkdb3')
        self.get_data_to_file('bkdb3', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_4", dbname = 'bkdb3', option = "--ddboost --no-owner ", location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb3', option = '--ddboost -l res_log -d %s -B 100 -e' % os.environ.get('MASTER_DATA_DIRECTORY'), location=self.BACKUPDIR)
        self.validate_restore("restore_dir", 'bkdb3')
        self.compare_table_data('bkdb3')

        self.compare_ao_tuple_count('bkdb3')

    def test_04_incremental_restore_select_tables_with_s(self):
        tinctest.logger.info("Test24: Test to restore with selected tables with option s ")

        self.run_workload("backup_dir", 'bkdb9')
        self.run_full_backup(dbname = 'bkdb9', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb9')
        self.get_data_to_file('bkdb9', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb9', option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb9', option = '-T public.ao_table1 -T public.heap_table3 -T public.mixed_part01 --ddboost -e -s bkdb9', location=self.BACKUPDIR)
        self.validate_restore("restore_incr_T_with_e", 'bkdb9')

    def test_05_incremental_restore_select_tablefile_with_option_s(self):
        tinctest.logger.info("Test25: Test to restore with selected tables in a file  with option s , drop and recreate db manualy before restore")

        self.run_workload("backup_dir", 'bkdb9')
        self.run_full_backup(dbname = 'bkdb9', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb9')
        self.get_data_to_file('bkdb9', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb9', option = '--ddboost', location=self.BACKUPDIR)

        self.drop_create_database('bkdb9')

        table_file = local_path('%s/table_file1' % ('restore_incr_T_with_e'))
        self.cleanup()
        self.run_restore('bkdb9', option = '--table-file=%s --ddboost -e -s bkdb9' % table_file, location=self.BACKUPDIR)
        self.validate_restore("restore_incr_T_with_e", 'bkdb9')

    def test_06_restore_scenarios_1(self):
        tinctest.logger.info("Test28: Test restore scenarios : full,incre1, incre2, restore incre1, incre3, restore incre3")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb17')
        self.run_full_backup(dbname = 'bkdb17', option = '--ddboost', location=self.BACKUPDIR)
        self.run_workload("dirty_dir_1", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb17', incr_no = 1, option = '--ddboost', location=self.BACKUPDIR)
        self.run_workload("dirty_dir_2", 'bkdb17')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb17', incr_no = 2, option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 1, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17')

        self.run_workload("dirty_dir_3", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_restore_1_and_3", dbname = 'bkdb17', incr_no = 3, option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost', incr_no = 3, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17')
        self.compare_ao_tuple_count('bkdb17')

    def test_07_restore_scenarios_2(self):
        tinctest.logger.info("Test29: Test restore scenarios : full,incre1, incre2, restore full, incre3, restore incre2")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb17')
        self.run_full_backup(dbname = 'bkdb17', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb17')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb17', incr_no = 1, option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb17', incr_no = 2, option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost' , type = 'full', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup2') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_restore_full_and_3", dbname = 'bkdb17', incr_no = 3, option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 2, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17')

        self.run_workload("backup_inserts" , 'bkdb17')
        self.run_full_backup(dbname = 'bkdb17', option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 3, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17', restore_no = 2)
        self.compare_ao_tuple_count('bkdb17')

    def test_08_restore_scenarios_3(self):
        tinctest.logger.info("Test30: Test restore scenarios : full,incre1, incre2, restore full, incre3, full, restore incre3")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_full_backup(dbname = 'bkdb17', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb17')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb17', incr_no = 1, option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb17')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb17', incr_no = 2, option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost' , type = 'full', location=self.BACKUPDIR)
        self.compare_table_data('bkdb17')
        self.compare_ao_tuple_count('bkdb17')

        self.run_workload("dirty_dir_3", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup2') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_restore_full_and_3", dbname = 'bkdb17', incr_no = 3, option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("backup_inserts" , 'bkdb17')
        self.run_full_backup(dbname = 'bkdb17', option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 3, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17', restore_no = 2)
        self.compare_ao_tuple_count('bkdb17')

    """
    def test_09_restore_scenarios_4(self):
        tinctest.logger.info("Test31: Test restore scenarios : full,incre1, incre2, incre3, incre4, restore full, restore incre2, restore incre3, restore incre4")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup1')
        self.run_full_backup(dbname = 'bkdb17', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup2')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb17', incr_no = 1, option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup3')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb17', incr_no = 2, option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup4')
        self.run_incr_backup("dirty_dir_3", dbname = 'bkdb17', incr_no = 3, option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_4", 'bkdb17')
        self.get_data_to_file('bkdb17', 'backup5')
        self.run_incr_backup("dirty_dir_4", dbname = 'bkdb17', incr_no = 4, option = '--ddboost', location=self.BACKUPDIR)

        self.run_restore('bkdb17', option = ' -e --ddboost' , type = 'full')
        self.compare_table_data('bkdb17')
        self.compare_ao_tuple_count('bkdb17')

        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 1, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17', restore_no = 2)
        self.compare_ao_tuple_count('bkdb17')

        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 2, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17', restore_no = 3)
        self.compare_ao_tuple_count('bkdb17')

        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 3, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17', restore_no = 4)
        self.compare_ao_tuple_count('bkdb17')

        self.run_restore('bkdb17', option = ' -e --ddboost' , incr_no = 4, location=self.BACKUPDIR)
        self.compare_table_data('bkdb17', restore_no = 5)
        self.compare_ao_tuple_count('bkdb17')
    """

    def test_10_large_dirty_list(self):
        tinctest.logger.info("Test32: gpcromndump --incremental with more than 1000 dirty tables...")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb1')
        self.run_full_backup(dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb1')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb1')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb1')
        self.run_incr_backup("dirty_dir_3", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_4", 'bkdb1')
        self.run_incr_backup("dirty_dir_4", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_5", 'bkdb1')
        self.get_data_to_file('bkdb1', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_5", dbname = 'bkdb1', option = '--ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb1', option = '--ddboost', location=self.BACKUPDIR)
        self.validate_restore("restore_dir", 'bkdb1')
        self.compare_table_data('bkdb1')
        self.compare_ao_tuple_count('bkdb17')

    def test_11_incremental_after_vacuum(self):
        tinctest.logger.info("Test 38: Negative: Full backup, Vacumm, incremental backup -should be an empty dirty_list")
        self.cleanup_backup_files()
        self.run_workload("backup_dir", 'bkdb38')
        self.run_full_backup(dbname = 'bkdb38', option = '--ddboost', location=self.BACKUPDIR)
        PSQL.run_sql_command('Vacuum;',dbname='bkdb38')
        self.run_incr_backup("dirty_dir_empty", dbname = 'bkdb38', option = '--ddboost', location=self.BACKUPDIR)

    def test_12_incremental_with_prefix(self):
        tinctest.logger.info("Test1: gpcromndump --incremental with no options...")

        self.run_workload("backup_dir", 'bkdb1')

        self.run_full_backup(dbname = 'bkdb1', option = '--prefix=foo --ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1", 'bkdb1')
        self.run_incr_backup("dirty_dir_1", dbname = 'bkdb1', option = '--prefix=foo --ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2", 'bkdb1')
        self.run_incr_backup("dirty_dir_2", dbname = 'bkdb1', option = '--prefix=foo --ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_3", 'bkdb1')
        self.run_incr_backup("dirty_dir_3", dbname = 'bkdb1', option = '--prefix=foo --ddboost', location=self.BACKUPDIR)

        self.run_workload("dirty_dir_4", 'bkdb1')
        self.get_data_to_file('bkdb1', 'backup1') #Create a copy of all the tables in database before the last backup
        self.run_incr_backup("dirty_dir_4", dbname = 'bkdb1', option = '--prefix=foo --ddboost', location=self.BACKUPDIR)

        self.cleanup()
        self.run_restore('bkdb1', option = '--prefix=foo --ddboost', location=self.BACKUPDIR)
        self.validate_restore("restore_dir", 'bkdb1')
        self.compare_table_data('bkdb1')

        self.compare_ao_tuple_count('bkdb1')

    def test_13_full_backup_restore_prefix_option(self):
        tinctest.logger.info("Test33: Test for verifying full backup and restore with option --prefix")
        self.cleanup_backup_files()

        self.run_workload("backup_dir", 'bkdb2')
        self.run_full_backup(dbname = 'bkdb2', option = "--prefix=foo --ddboost", location=self.BACKUPDIR)
        self.check_filter_file_exists(expected=False, prefix='foo')

        self.get_data_to_file('bkdb2', 'backup1') #Create a copy of all the tables in database before the last backup

        self.run_restore('bkdb2', option = '-s bkdb2 -e --prefix foo --ddboost', location=self.BACKUPDIR)
        self.compare_table_data('bkdb2')
        self.compare_ao_tuple_count('bkdb2')

    def test_14_incremental_backup_restore_prefix_option(self):
        tinctest.logger.info("Test34: Test for verifying incremental backup and restore with option --prefix")
        self.cleanup_backup_files()

        self.run_workload("backup_dir_less_tables", 'bkdb34')
        self.run_full_backup(dbname = 'bkdb34', option = "--prefix=foo --ddboost", location=self.BACKUPDIR)
        self.check_filter_file_exists(expected=False, prefix='foo')

        self.run_workload("dirty_dir_1_less_tables", 'bkdb34')
        self.run_incr_backup("dirty_dir_1_less_tables", dbname = 'bkdb34',  option = "--prefix=foo --ddboost", location=self.BACKUPDIR)

        self.get_data_to_file('bkdb34', 'backup1') #Create a copy of all the tables in database before the last backup

        self.run_restore('bkdb34', option = '-s bkdb34 -e --prefix foo --ddboost', location=self.BACKUPDIR)
        self.compare_table_data('bkdb34')
        self.compare_ao_tuple_count('bkdb34')

    def test_15_full_backup_restore_filtering(self):
        tinctest.logger.info("Test35: Test for verifying full backup and restore with options --prefix and -t")
        self.cleanup_backup_files()

        self.run_workload("backup_dir_less_tables", 'bkdb35')
        self.run_full_backup(dbname = 'bkdb35', option = "--prefix=foo -t public.ao_table21 --ddboost", location=self.BACKUPDIR)

        self.get_data_to_file('bkdb35', 'backup1') #Create a copy of all the tables in database before the last backup

        self.run_restore('bkdb35', option = '-s bkdb35 -e --prefix foo --ddboost', location=self.BACKUPDIR)
        self.compare_table_data('bkdb35')
        self.compare_ao_tuple_count('bkdb35')

    def test_16_incremental_backup_restore_filtering(self):
        tinctest.logger.info("Test36: Test for verifying incremental backup and restore for a full backup with options --prefix and -t")
        self.cleanup_backup_files()

        self.run_workload("backup_dir_less_tables", 'bkdb36')
        self.run_full_backup(dbname = 'bkdb36', option = "--prefix=foo -t public.ao_table21 --ddboost", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1_filtering", 'bkdb36')
        self.run_incr_backup("dirty_dir_1_filtering", dbname = 'bkdb36',  option = "--prefix=foo --ddboost", location=self.BACKUPDIR)

        self.get_data_to_file('bkdb36', 'backup1') #Create a copy of all the tables in database before the last backup

        self.run_restore('bkdb36', option = '-s bkdb36 -e --prefix foo --ddboost', location=self.BACKUPDIR)
        self.compare_table_data('bkdb36')
        self.compare_ao_tuple_count('bkdb36')

    def test_17_multiple_prefixes_incr_filtering(self):
        tinctest.logger.info("Test37: Test for verifying multiple incremental backup and restore with filtering and different prefixes")
        self.cleanup_backup_files()

        self.run_workload("backup_dir_less_tables", 'bkdb37_1')
        self.run_full_backup(dbname = 'bkdb37_1', option = "--prefix=foo1 -t public.ao_table21 --ddboost", location=self.BACKUPDIR)

        self.run_workload("backup_dir_less_tables", 'bkdb37_2')
        self.run_full_backup(dbname = 'bkdb37_2', option = "--prefix=foo2 -t public.ao_table20 --ddboost", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_1_filtering", 'bkdb37_1')
        self.run_incr_backup("dirty_dir_1_filtering", dbname = 'bkdb37_1',  option = "--prefix=foo1 --ddboost", location=self.BACKUPDIR)

        self.run_workload("dirty_dir_2_filtering", 'bkdb37_2')
        self.run_incr_backup("dirty_dir_2_filtering", dbname = 'bkdb37_2',  option = "--prefix=foo2 --ddboost", location=self.BACKUPDIR)

        self.get_data_to_file('bkdb37_1', 'backup1') #Create a copy of all the tables in database before the last backup
        self.get_data_to_file('bkdb37_2', 'backup2') #Create a copy of all the tables in database before the last backup

        self.run_restore('bkdb37_1', option = '-s bkdb37_1 -e --prefix foo1 --ddboost', location=self.BACKUPDIR)
        self.run_restore('bkdb37_2', option = '-s bkdb37_2 -e --prefix foo2 --ddboost', location=self.BACKUPDIR)
        self.compare_table_data('bkdb37_1')
        self.compare_ao_tuple_count('bkdb37_1')
        self.compare_table_data('bkdb37_2')
        self.compare_ao_tuple_count('bkdb37_2')

    def test_18_ddboost_cleanup(self):
        cmd_del_dir = "gpddboost --del-dir=%s" % self.BACKUPDIR
        (err, out) = self.run_command(cmd_del_dir)
        if err:
            msg = "Failed to delete dir %s on DD \n" % (self.BACKUPDIR)
            raise Exception('Error: %s\n%s'% (msg,out))

    def test_ddboost_dynamic_storage_unit(self):
        """
        Note: storage unit TEMP is assumed to already exist on DD server.
        """
        default_dbname = 'gptest'

        if not self.check_db_exists('gptest'):
            self.create_database('gptest')

        # get some tables created to perform test
        self.cleanup_simple_tables(default_dbname)
        self.populate_simple_tables(default_dbname)

        # perform the full dump
        dump_to_dd_storage_unit_option = '--ddboost --ddboost-storage-unit %s' % alternative_storage_unit
        self.run_gpcrondump(dbname='gptest', option = dump_to_dd_storage_unit_option)

        # get a list of full dumped files from DD server
        list_full_dumped_files= "gpddboost --ls '/%s/%s/' --ddboost-storage-unit=%s" % (self.BACKUPDIR, self.full_backup_timestamp[:8], alternative_storage_unit)
        _, stdout = self.run_command(list_full_dumped_files)
        full_dumped_files = self.get_list_of_ddboost_files(stdout, self.full_backup_timestamp)

        # assert the first time a dump file should exist on DD server using dynamic storage unit option
        self.assertTrue(len(full_dumped_files) > 0)

        # populate some dirty data for incremental backup
        self.populate_simple_tables_data(default_dbname)

        # perform the incremental dump
        incr_dump_to_dd_storage_unit_option = '--ddboost --ddboost-storage-unit %s --incremental ' % alternative_storage_unit
        self.run_gpcrondump(dbname='gptest', option = dump_to_dd_storage_unit_option)

        # get a list of incremental dumped files from DD server
        list_incr_dumped_files= "gpddboost --ls '/%s/%s/' --ddboost-storage-unit=%s" % (self.BACKUPDIR, self.backup_timestamp[:8], alternative_storage_unit)
        _, stdout = self.run_command(list_incr_dumped_files)
        incr_dumped_files = self.get_list_of_ddboost_files(stdout, self.backup_timestamp)

        # cleanup and perform restore, by default it uses the latest incremental dump timestamp
        self.cleanup_simple_tables(default_dbname)
        self.run_restore(default_dbname, type='incremental', option = ' --ddboost --ddboost-storage-unit %s ' % alternative_storage_unit)

        # verify the row count after restore matches
        self.verifying_simple_tables(default_dbname, {'ao':1, 'co':1, 'heap':1})

        # cleanup the dumped file (both full dump files and incremental dump files) on DD server under storage unit(TEMP)
        self.delete_ddboost_files(os.path.join('/', self.BACKUPDIR, self.full_backup_timestamp[:8]), full_dumped_files, alternative_storage_unit)
        self.delete_ddboost_files(os.path.join('/', self.BACKUPDIR, self.backup_timestamp[:8]), incr_dumped_files, alternative_storage_unit)

    def test_ddboost_dynamic_storage_unit_noplan(self):
        """
        Run dynamic storage incremental restore with noplan option
        """
        default_dbname = 'gptest'

        if not self.check_db_exists('gptest'):
            self.create_database('gptest')

        # get some tables created to perform test
        self.cleanup_simple_tables(default_dbname)
        self.populate_simple_tables(default_dbname)
        self.populate_simple_tables_data(default_dbname)

        # perform the full dump
        dump_to_dd_storage_unit_option = '--ddboost --ddboost-storage-unit %s' % alternative_storage_unit
        self.run_gpcrondump(dbname='gptest', option = dump_to_dd_storage_unit_option)

        # get a list of full dumped files from DD server
        list_full_dumped_files= "gpddboost --ls '/%s/%s/' --ddboost-storage-unit=%s" % (self.BACKUPDIR, self.full_backup_timestamp[:8], alternative_storage_unit)
        _, stdout = self.run_command(list_full_dumped_files)
        full_dumped_files = self.get_list_of_ddboost_files(stdout, self.full_backup_timestamp)

        # assert the first time a dump file should exist on DD server using dynamic storage unit option
        self.assertTrue(len(full_dumped_files) > 0)

        # populate some dirty data for incremental backup
        self.populate_simple_tables_data(default_dbname)

        # perform the incremental dump
        incr_dump_to_dd_storage_unit_option = '--ddboost --ddboost-storage-unit %s --incremental ' % alternative_storage_unit
        self.run_gpcrondump(dbname='gptest', option = dump_to_dd_storage_unit_option)

        # get a list of incremental dumped files from DD server
        list_incr_dumped_files= "gpddboost --ls '/%s/%s/' --ddboost-storage-unit=%s" % (self.BACKUPDIR, self.backup_timestamp[:8], alternative_storage_unit)
        _, stdout = self.run_command(list_incr_dumped_files)
        incr_dumped_files = self.get_list_of_ddboost_files(stdout, self.backup_timestamp)

        # cleanup and perform restore, by default it uses the latest (first) incremental dump timestamp: self.backup_timestamp
        restore_cmdStr =  'gpdbrestore -t %s --ddboost --ddboost-storage-unit %s --noplan --truncate -a' % (self.backup_timestamp, alternative_storage_unit)
        self.run_command(restore_cmdStr)

        # verify the row count after restore matches
        self.verifying_simple_tables(default_dbname, {'ao':2, 'co':2, 'heap':2})

        # cleanup the dumped file (both full dump files and incremental dump files) on DD server under storage unit(TEMP)
        self.delete_ddboost_files(os.path.join('/', self.BACKUPDIR, self.full_backup_timestamp[:8]), full_dumped_files, alternative_storage_unit)
        self.delete_ddboost_files(os.path.join('/', self.BACKUPDIR, self.backup_timestamp[:8]), incr_dumped_files, alternative_storage_unit)

