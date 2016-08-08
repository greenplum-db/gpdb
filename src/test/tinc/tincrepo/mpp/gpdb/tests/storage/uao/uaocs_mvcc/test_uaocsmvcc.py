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

import tinctest
import re
from tinctest.models.scenario import ScenarioTestCase
from tinctest.lib import local_path, Gpdiff
from mpp.lib.PSQL import PSQL, PSQLException
from tinctest.main import TINCException

class MVCC_UAOCS_Exception( TINCException ): pass
class MVCC_UAOCS_TestCase(ScenarioTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    def __init__(self, methodName):
        super(MVCC_UAOCS_TestCase,self).__init__(methodName)

    @classmethod
    def setUpClass(cls):
        #Create tables that will be used for tests below
        super(MVCC_UAOCS_TestCase, cls).setUpClass()
        sql_file=local_path('setup/create_tab_foruaocsmvcc.sql')
        out_file=local_path('setup/create_tab_foruaocsmvcc.out')

    	print "sql file for creating tables : "+ (sql_file)
    	print "output file for sqls         : "+ (out_file)
	assert PSQL.run_sql_file(sql_file = sql_file,out_file = out_file,flags='-q')

    def NoRun_test_readcommit_concurrentupdate(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurrentupdate.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurrentupdate' and t2.workload='readcommit_concurrentupdate' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	
    def NoRun_test_readcommit_concurrentupdate_del(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurrentupdate_del.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurrentupdate_delete' and t2.workload='readcommit_concurrentupdate_delete' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	
    

    def NoRun_test_readcommit_concurrentupdate_rollback(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurrentupdate_rollback.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurrentupdate_rollback' and t2.workload='readcommit_concurrentupdate_rollback' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    
    def NoRun_test_readcommit_concurrentupdate_del_rollback(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurrentupdate_del_rollback.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurrentupdate_delete_rollback' and t2.workload='readcommit_concurrentupdate_delete_rollback' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoReun_test_readcommit_concurr_ins_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurr_ins_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurr_ins_vacuum' and t2.workload='readcommit_concurr_ins_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readcommit_concurr_upd_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurr_upd_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurr_upd_vacuum' and t2.workload='readcommit_concurr_upd_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readcommit_concurr_alter_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurr_alter_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurr_alter_vacuum' and t2.workload='readcommit_concurr_alter_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readcommit_concurr_upd_savepoint(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurr_upd_savepoint.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurrentupdate_savepoint' and t2.workload='readcommit_concurrentupdate_savepoint' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readcommit_concurr_vacuum_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurr_vacuum_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurr_vacuum_vacuum' and t2.workload='readcommit_concurr_vacuum_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	
    def NoRun_test_readcommit_concurr_droptab_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readcommit.readcommit_concurr_droptab_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readcommit_concurr_drop_vacuum' and t2.workload='readcommit_concurr_drop_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readserial_concurrentupdate(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurrentupdate.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurrentupdate' and t2.workload='readserial_concurrentupdate' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readserial_concurrent_upd_ins(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurrent_upd_ins.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurrent_upd_ins' and t2.workload='readserial_concurrent_upd_ins' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	


    def NoRun_test_readserial_concurrentdelete(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurrentdelete.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurrentdelete' and t2.workload='readserial_concurrentdelete' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readserial_concurrent_del_ins(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurrent_del_ins.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurrent_del_ins' and t2.workload='readserial_concurrent_del_ins' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	

    def NoRun_test_readserial_concurr_upd_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurr_upd_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurr_upd_vacuum' and t2.workload='readserial_concurr_upd_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	
    def NoRun_test_readserial_concurr_alter_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurr_alter_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurr_alter_vacuum' and t2.workload='readserial_concurr_alter_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	


    def NoRun_test_readserial_concurr_ins_vacuum(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurr_ins_vacuum.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list1)
        sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurr_ins_vacuum' and t2.workload='readserial_concurr_ins_vacuum' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
        self.assertIsNotNone(re.search('1 row', sql_out))

    def NoRun_test_readserial_concurr_upd_savepoint(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.uao.uaocs_mvcc.readserial.readserial_concurr_upd_savepoint.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list0)
	sql_cmd="select count(*) from sto_uaocs_mvcc_status t1 , sto_uaocs_mvcc_status t2 where t1.workload='readserial_concurrentupdate_savepoint' and t2.workload='readserial_concurrentupdate_savepoint' and t1.script != t2.script and t1.starttime < t2.starttime and t2.starttime < t1.endtime"
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))	
