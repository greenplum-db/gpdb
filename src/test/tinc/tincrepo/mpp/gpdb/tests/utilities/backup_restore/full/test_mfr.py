#!/usr/bin/env python

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
import pexpect
from datetime import date
import unittest2 as unittest
import tinctest
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.utilities.backup_restore.full import BackupTestCase, is_earlier_than
from tinctest.lib import local_path

import sys, string, time
from mpp.lib.gpdb_util import GPDBUtil
gpdb_util = GPDBUtil()
version = gpdb_util.getGPDBVersion()

#Fix the stdout.write so it flushes automatically
unbuffered = os.fdopen(sys.stdout.fileno(), 'w', 0)
sys.stdout = unbuffered
port = os.environ.get('PGPORT')

#Fix the search path for modules so it finds the utility lib dir
MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)
UPD = os.path.abspath(mkpath('..'))

if UPD not in sys.path:
    sys.path.append(UPD)

if MYD in sys.path:
    sys.path.remove( MYD )
    sys.path.append( MYD )

sys.path.append( MYD + os.sep + "lib" )
sys.path.append( UPD + os.sep + "lib" )

'''
@Full Backup
'''

ALTERNATIVE_STORAGE_UNIT = 'TEMP'

class test_mfr(BackupTestCase):
    """

    @description test cases for incremental backup with ddboost
    @created 2014-08-10 10:10:10
    @modified 2014-08-11 10:10:15
    @tags backup restore gpcrondump gpdbrestore mfr
    @product_version gpdb: [4.3.2.x- main]
    @gucs gp_create_table_random_default_distribution=off
    """

    DB3DumpKey = None
    DB2DumpKey = None
    DB1DumpKey = None

    def __init__(self, methodName):
        super(test_mfr, self).__init__(methodName)

    def test_01_ddboost_config(self):
        cmdOutFile = 'test01_config_ddboost_with_mfr_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        cmdRepl = "gpcrondump --ddboost-config-remove"
        (rc, out) = self.run_command(cmdRepl)

        if rc!=0:
            msg = "Failed to remove previous DD configuration\n" % backup_set
            for i in out:
                msg += i

            raise Exception(msg)

        cfg_cmd_1 = "gpcrondump --ddboost-host %s --ddboost-user %s --ddboost-backupdir %s" %\
        (BackupTestCase.TSTINFO['DDBOOST_HOST_1'], BackupTestCase.TSTINFO['DDBOOST_USER_1'], BackupTestCase.TSTINFO['DDBOOST_DIR'])

        local = pexpect.spawn(cfg_cmd_1)
        local.expect('Password: ')
        local.sendline(BackupTestCase.TSTINFO['DDBOOST_PASSWORD_1'])
        local.expect(pexpect.EOF)
        local.close()

        cfg_cmd_2 = "gpcrondump --ddboost-host %s --ddboost-user %s --ddboost-backupdir %s --ddboost-remote" %\
        (BackupTestCase.TSTINFO['DDBOOST_HOST_2'], BackupTestCase.TSTINFO['DDBOOST_USER_2'], BackupTestCase.TSTINFO['DDBOOST_DIR'])

        remote = pexpect.spawn(cfg_cmd_2)
        remote.expect('Password: ')
        remote.sendline(BackupTestCase.TSTINFO['DDBOOST_PASSWORD_2'])
        remote.expect(pexpect.EOF)
        remote.close()

    def test_02_gpcrondump_with_replicate(self):
        #Conduct full backup of database with replication
        global DB1DumpKey
        DBNAME = BackupTestCase.TSTINFO['DB1']
        (DB1DumpKey, errmsg) = self.do_bkup_test('test02_gpcrondump_with_replicate',DBNAME,'--replicate')

        if DB1DumpKey == None:
            raise Exception(errmsg)

    def test_03_gpcrondump_without_replicate(self):
        #Conduct full backup of database without replication
        global DB2DumpKey
        DBNAME = BackupTestCase.TSTINFO['DB2']
        (DB2DumpKey, errmsg) = self.do_bkup_test('test03_gpcrondump_without_replicate',DBNAME,None)

        if DB2DumpKey == None:
            raise Exception(errmsg)

    def test_04_gpcrondump_without_replicate(self):
        #Conduct full backup of database without replication
        global DB3DumpKey
        DBNAME = BackupTestCase.TSTINFO['DB2']
        (DB3DumpKey, errmsg) = self.do_incr_bkup_test('test03_gpcrondump_without_replicate',DBNAME,None)

        if DB3DumpKey == None:
            raise Exception(errmsg)

    def test_05_replicate_backup(self):
        global DB2DumpKey
        backup_set = self.convert_dmpkey_to_date(DB2DumpKey)

        cmdOutFile = 'test05_replicate_backup_replicate_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        cmdRepl = "gpmfr.py --replicate=\"%s\" --max-streams=15 --master-port=%s" % (backup_set, port)
        #(ok,out) = shell.run(cmdRepl,cmdOut,'w')
        (rc, out) = self.run_command(cmdRepl)
        if rc!=0:
            msg = "Replication of backup set %s failed\n" % backup_set
            for i in out:
                msg += i

            raise Exception(msg)

    def test_06_replicate_backup(self):
        global DB3DumpKey
        backup_set = self.convert_dmpkey_to_date(DB3DumpKey)

        cmdOutFile = 'test05_replicate_backup_replicate_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        cmdRepl = "gpmfr.py --replicate=\"%s\" --max-streams=15 --master-port=%s" % (backup_set, port)
        #(ok,out) = shell.run(cmdRepl,cmdOut,'w')
        (rc, out) = self.run_command(cmdRepl)
        if rc!=0:
            msg = "Replication of backup set %s failed\n" % backup_set
            for i in out:
                msg += i

            raise Exception(msg)

    def test_07_list_backup_sets_local_ddr(self):
        #List the backup sets on the local DD
        global DB1DumpKey
        global DB2DumpKey

        (ok,errmsg) = self.mfr_list_sets('test06_list_backup_sets_local_ddr',DB1DumpKey,DB2DumpKey)

        if not ok:
            msg = "Local backup set list on local DD failed\n"
            msg += errmsg

            raise Exception(msg)

    def test_08_list_files_in_backup_sets_local_ddr(self):
        #List the file in the backup sets on the local DD
        global DB1DumpKey
        global DB2DumpKey

        (ok,errmsg) = self.mfr_list_files('test07_list_files_in_backup_sets_local_ddr',DB1DumpKey,DB2DumpKey,'file')

        if not ok:
            msg = "Local backup set file list on local DD failed\n"
            msg += errmsg

            raise Exception(msg)

    def test_09_list_backup_set_remote_ddr(self):
        #List the backup sets on the remote DD
        global DB1DumpKey
        global DB2DumpKey

        (ok,errmsg) = self.mfr_list_sets('test08_list_backup_set_remote_ddr',DB1DumpKey,DB2DumpKey,remote=True)

        if not ok:
            msg = "Remote backup set list on remote DD failed\n"
            msg += errmsg

            raise Exception(msg)

    def test_10_list_files_in_backup_sets_remote_ddr(self):
        #List the files in the backup sets on the remote DD
        global DB1DumpKey
        global DB2DumpKey

        (ok,errmsg) = self.mfr_list_files('test09_list_files_in_backup_sets_remote_ddr',DB1DumpKey,DB2DumpKey,'file',remote=True)

        if not ok:
            msg = "Remote backup set file list on remote DD failed\n"
            msg += errmsg

            raise Exception(msg)

    def test_11_gpdbrestore_using_ddboost(self):
        #Restore full backup of database that used replicate option
        global DB1DumpKey

        DBNAME = BackupTestCase.TSTINFO['DB1']
        if DB1DumpKey == None:
            self.skipTest(msg="Skipping test due to test02 failure")
        else:
            (status,errmsg) = self.do_restr_test('test10_gpdbrestore_using_ddboost',DBNAME,DB1DumpKey)

            if status == 'FAILED':
                raise Exception(errmsg)

    def test_12_delete_primary_ddr_backup_set(self):
        #Remove the backup set for database on primary DD
        global DB2DumpKey
        backup_set = self.convert_dmpkey_to_date(DB2DumpKey)

        del_cmd = "gpmfr.py --delete=\"%s\" --master-port=%s" % (backup_set, port)

        local = pexpect.spawn(del_cmd)
        local.expect('(y/n)?')
        local.sendline('y')
        local.expect(pexpect.EOF,timeout=300)
        local.close()

    def test_13_delete_primary_ddr_backup_set(self):
        #Remove the backup set for database on primary DD
        global DB3DumpKey
        backup_set = self.convert_dmpkey_to_date(DB3DumpKey)

        del_cmd = "gpmfr.py --delete=\"%s\" --master-port=%s" % (backup_set, port)

        local = pexpect.spawn(del_cmd)
        local.expect('(y/n)?')
        local.sendline('y')
        local.expect(pexpect.EOF,timeout=300)
        local.close()

    def test_14_recover_backup_set_from_secondary_ddr(self):
        #Recover the backup set for database from remote DD
        global DB2DumpKey
        backup_set = self.convert_dmpkey_to_date(DB2DumpKey)

        cmdOutFile = 'test12__recover_backup_set_from_secondary_ddr_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        cmdRepl = "gpmfr.py --recover=\"%s\" --max-streams=15 --master-port=%s" % (backup_set, port)
        #(ok,out) = shell.run(cmdRepl,cmdOut,'w')
        (rc, out) = self.run_command(cmdRepl)
        if rc!=0:
            msg = "Recovery of backup set %s failed\n" % backup_set
            for i in out:
                msg += i

            raise Exception(msg)

    def test_15_recover_backup_set_from_secondary_ddr(self):
        #Recover the backup set for database from remote DD
        global DB3DumpKey
        backup_set = self.convert_dmpkey_to_date(DB3DumpKey)

        cmdOutFile = 'test12__recover_backup_set_from_secondary_ddr_'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        cmdRepl = "gpmfr.py --recover=\"%s\" --max-streams=15 --master-port=%s" % (backup_set, port)
        #(ok,out) = shell.run(cmdRepl,cmdOut,'w')
        (rc, out) = self.run_command(cmdRepl)
        if rc!=0:
            msg = "Recovery of backup set %s failed\n" % backup_set
            for i in out:
                msg += i

            raise Exception(msg)

    def test_16_gpdbrestore_using_ddboost(self):
        #Restore full backup of database backup set just recovered from remote DD
        global DB2DumpKey

        DBNAME = BackupTestCase.TSTINFO['DB2']

        if DB2DumpKey == None:
            self.skipTest(msg="Skipping test due to test03 failure")
        else:
            (status,errmsg) = self.do_restr_test('test13_gpdbrestore_using_ddboost',DBNAME,DB2DumpKey)

            if status == 'FAILED':
                raise Exception(errmsg)

    def test_17_gpdbrestore_using_ddboost(self):
        #Restore full backup of database backup set just recovered from remote DD
        global DB3DumpKey

        DBNAME = BackupTestCase.TSTINFO['DB2']

        if DB2DumpKey == None:
            self.skipTest(msg="Skipping test due to test03 failure")
        else:
            (status,errmsg) = self.do_restr_test('test13_gpdbrestore_using_ddboost',DBNAME,DB3DumpKey)
            if status == 'FAILED':
                raise Exception(errmsg)

    def test_18_delete_secondary_ddr_backup_set(self):
        #Remove backup set on remote DD
        global DB1DumpKey
        backup_set = self.convert_dmpkey_to_date(DB1DumpKey)

        del_cmd = "gpmfr.py --delete=\"%s\" --remote --master-port=%s" % (backup_set, port)

        local = pexpect.spawn(del_cmd)
        local.expect('(y/n)?')
        local.sendline('y')
        local.expect(pexpect.EOF, timeout=300)
        local.close()

    def test_19_replicate_backup_set(self):
        #Replicate the backup sets to the remote DD
        global DB1DumpKey
        backup_set = self.convert_dmpkey_to_date(DB1DumpKey)

        cmdOutFile = 'test_replicate_backup_set_15'+time.strftime("_%d%m%Y-%H%M%S", time.gmtime())+'.out'
        cmdOut = mkpath(cmdOutFile)
        cmdRepl = "gpmfr.py --replicate=\"%s\" --max-streams=15 --master-port=%s" % (backup_set, port)
        #(ok,out) = shell.run(cmdRepl,cmdOut,'w')
        (rc, out) = self.run_command(cmdRepl)
        if rc!=0:
            msg = "Replication of backup set %s failed\n" % backup_set
            for i in out:
                msg += i

            raise Exception(msg)

    def test_20_list_latest_local_backup_set(self):
        #List the files in the backup set using LATEST parameter on local DD
        global DB1DumpKey
        global DB2DumpKey

        (ok,errmsg) = self.mfr_list_files('test_list_latest_local_backup_set_16',DB1DumpKey,DB2DumpKey,'latest')

        if not ok:
            msg = "Latest backup set file list on local DD failed\n"
            msg += errmsg

            raise Exception(msg)

    def test_21_list_latest_remote_backup_set(self):
        #List the files in the backup set using LATEST parameter on remote DD
        global DB1DumpKey
        global DB2DumpKey

        (ok,errmsg) = self.mfr_list_files('test_list_latest_remote_backup_set',DB1DumpKey,DB2DumpKey,'latest',remote=True)

        if not ok:
            msg = "Latest backup set file list on remote DD failed\n"
            msg += errmsg

            raise Exception(msg)

    def test_22_dynamic_storage_unit_with_replicate_and_recover(self):
        default_dbname = 'gptest'
        if not self.check_db_exists('gptest'):
            self.create_database('gptest')

        # create some table with data for test
        self.cleanup_simple_tables(default_dbname)
        self.populate_simple_tables(default_dbname)
        self.populate_simple_tables_data(default_dbname)

        # run full dump using the ALTERNATIVE_STORAGE_UNIT: TEMP
        dump_to_dd_storage_unit_option = '--ddboost --ddboost-storage-unit %s --replicate --max-streams 15' % ALTERNATIVE_STORAGE_UNIT
        self.run_gpcrondump(dbname='gptest', option = dump_to_dd_storage_unit_option)

        # get list of dumped files on primary DD server
        primary_list_dumped_files= "gpddboost --ls '/%s/%s/' --ddboost-storage-unit=%s" % (BackupTestCase.TSTINFO['DDBOOST_DIR'], self.full_backup_timestamp[:8], ALTERNATIVE_STORAGE_UNIT)
        _, stdout = self.run_command(primary_list_dumped_files)
        primary_dumped_files = self.get_list_of_ddboost_files(stdout, self.full_backup_timestamp)
        self.assertTrue(len(primary_dumped_files) > 0)

        # get list of dumped files on secondary DD server
        secondary_list_dumped_files= "gpddboost --ls '/%s/%s/' --ddboost-storage-unit=%s --remote" % (BackupTestCase.TSTINFO['DDBOOST_DIR'], self.full_backup_timestamp[:8], ALTERNATIVE_STORAGE_UNIT)
        _, stdout = self.run_command(secondary_list_dumped_files)
        secondary_dumped_files = self.get_list_of_ddboost_files(stdout, self.full_backup_timestamp)
        self.assertTrue(len(secondary_dumped_files) > 0)

        #verify that the dump file exist both on primary and secondary DD server
        self.assertEqual(primary_dumped_files, secondary_dumped_files)

        #-------------------- Run recover test using the timestamp from above replicate ------------------------
        # remove the primary dump directory
        self.delete_ddboost_files(os.path.join(BackupTestCase.TSTINFO['DDBOOST_DIR'], self.full_backup_timestamp[:8]),
                                  primary_dumped_files, ALTERNATIVE_STORAGE_UNIT, 'local')
        dd_recover_cmd = 'gpmfr --recover %s --ddboost-storage-unit %s --max-streams 15 -a' % (self.full_backup_timestamp, ALTERNATIVE_STORAGE_UNIT)
        self.run_command(dd_recover_cmd)

        # get list of dumped files on primary DD server
        _, stdout = self.run_command(primary_list_dumped_files)
        primary_dumped_files = self.get_list_of_ddboost_files(stdout, self.full_backup_timestamp)

        #verify that the dump file exist both on primary and secondary DD server
        self.assertEqual(primary_dumped_files, secondary_dumped_files)

        # cleanup
        self.delete_ddboost_files(os.path.join(BackupTestCase.TSTINFO['DDBOOST_DIR'], self.full_backup_timestamp[:8]),
                                  primary_dumped_files, ALTERNATIVE_STORAGE_UNIT, 'local')
        self.delete_ddboost_files(os.path.join(BackupTestCase.TSTINFO['DDBOOST_DIR'], self.full_backup_timestamp[:8]),
                                  secondary_dumped_files, ALTERNATIVE_STORAGE_UNIT, 'remote')

    def test_23_remove_backup_dir(self):
        cmd_check_dir = "gpddboost --listDirectory --dir=%s" % BackupTestCase.TSTINFO['DDBOOST_DIR']
        (err, out) = self.run_command(cmd_check_dir)
        if err:
            return
        cmd_del_dir = "gpddboost --del-dir=%s" % BackupTestCase.TSTINFO['DDBOOST_DIR']
        (err, out) = self.run_command(cmd_del_dir)
        if err:
            msg = "Failed to delete backup directory %s\n" % BackupTestCase.TSTINFO['DDBOOST_DIR']
            raise Exception('Error: %s\n%s'% (msg,out))
