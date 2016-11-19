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
import os

import time

from datetime import datetime

import unittest2 as unittest
from mpp.gpdb.tests.storage.lib import Database
from tinctest.models.scenario import ScenarioTestCase
from tinctest.lib import local_path, Gpdiff
from mpp.lib.PSQL import PSQL, PSQLException
from tinctest.main import TINCException

'''
Automation of visimap testing for UAO feature
'''
class uao_visimapException( TINCException ): pass

class uao_visimap(ScenarioTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """
    sqlpath=local_path('sql')
    outpath=local_path('output')
    anspath=local_path('expected')

    def __init__(self, methodName):
	self.gphome = os.environ.get('GPHOME')
	if self.gphome is None:
		raise uao_visimapException ("GPHOME environment variable is not set")
        super(uao_visimap,self).__init__(methodName)

    def setUp(self):
        super(uao_visimap, self).setUp()
	self.create_udf_get_gp_aocsseg()
	self.create_udf_gp_aovisimap_hidden_info()
	self.create_udf_gp_aovisimap()

    @classmethod
    def setUpClass(cls):
        super(uao_visimap, cls).setUpClass()
	sqlpath=local_path('sql')
	outpath=local_path('output')
	anspath=local_path('expected')
        if not os.path.exists(outpath):
               os.mkdir(outpath)

    def test_07_create_udf_gp_aovisimap_hidden_info(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_07 Verify the creation of UDF gp_aovisimap_hidden test run')
        tinctest.logger.info("-------------------------------\n")
        self.create_udf_gp_aovisimap_hidden_info()


    def test_19_create_udf_gp_aovisimap(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_19 Verify the creation of UDF gp_aovisimap test run')
        tinctest.logger.info("-------------------------------\n")
	self.create_udf_gp_aovisimap()

    def test_22_create_udf_get_gp_aocsseg(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_22 Verify the creation of UDF udf_get_gp_aocsseg test run')
        tinctest.logger.info("-------------------------------\n")
	self.create_udf_get_gp_aocsseg()

    def test_01_create_uao_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_01 Verify the creation of uao table for the test run')
        tinctest.logger.info("-------------------------------\n")
        sql_file = os.path.join(self.sqlpath,'create_uao_table_01.sql')
        out_file = os.path.join(self.outpath,'create_uao_table_01.out')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        if out_file[-2:] == '.t':
            out_file = out_file[:-2]
        if self.anspath is not None:
	    ans_file= os.path.join(self.anspath,'create_uao_table_01.ans')
            tinctest.logger.info("**** _run_sql_file() : ans file = "+ans_file)
            assert Gpdiff.are_files_equal(out_file, ans_file)
	os.remove(os.path.join('.', out_file))

    def test_02_check_empty_visimap_for_uao_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_02 Verify Empty visimap for  uao table for the test run')
        tinctest.logger.info("-------------------------------\n")
	tablename="uao_tab_test1"
	relid = self.get_relid(file_name=tablename )
	sql_cmd="SELECT 1 FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=%s" % relid
	sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
	self.assertIsNotNone(re.search('1 row', sql_out))

    def test_03_use_udf_gp_aovisimap_hidden_info_uaocs_upd(self):
        tablename ='uaocs_table_test14'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_03 Verify the hidden tup_count using udf gp_aovisimap_hidden_info(oid)  for uaocs relation after update ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.ans')
        #create uaocs table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
        #get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_uaocs_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]
        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

        # update  row
        sql_cmd3="update %s set j = 'test_update' ;" % (tablename)
        sql_out=PSQL.run_sql_command(sql_cmd=sql_cmd3)
        #sql_out=PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')
        #relid = self.get_relid(file_name=tablename )
        num=(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port))
        assert(int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)) > 0)

        self.vacuum_full(tablename=tablename)
        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))


    def test_04_guc_select_invisible_for_uao_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_04 Verify GUC select_invisible for  uao table for the test run')
        tinctest.logger.info("-------------------------------\n")
	tablename ='uao_tab_test1'
	relid = self.get_relid(file_name=tablename )
	sql_file = os.path.join(self.sqlpath,'guc_selinvisibletrue_04.sql')
        out_file = os.path.join(self.outpath,'guc_selinvisibletrue_04.out')
	ans_file= os.path.join(self.anspath,'guc_selinvisibletrue_04.ans')
	PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, flags='-q')
        assert Gpdiff.are_files_equal(out_file, ans_file)


    def test_05_truncatetab_visimap_for_uao_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_05 Verify that the visimap truncates with truncate uao table test run')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'truncatevisimapinfo_05.out')
        sql_file = os.path.join(self.sqlpath,'truncatevisimapinfo_05.sql')
        ans_file= os.path.join(self.anspath,'truncatevisimapinfo_05.ans')
        sql_cmd1="drop table if exists uao_visimap_test05;create table uao_visimap_test05 (i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);\n"
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
	self.assertIsNotNone(re.search('CREATE TABLE', sql_out))
        sql_cmd2="\\pset tuples_only\n\\pset footer off\nSELECT relfilenode FROM pg_class WHERE relname='uao_visimap_test05';\n"
        with open(sql_file,'w') as f:
                f.write(sql_cmd2);
        sql_out=PSQL.run_sql_file(sql_file=sql_file, out_file=out_file,  flags='-q')
        with open(out_file, 'r') as f:
            relid = f.read()
	aovisimap_cmd="select count(*) from gp_dist_random('pg_aoseg.pg_aovisimap_%s');\n" % relid.strip()
	sql_cmd3="select * from uao_visimap_test05;\n"+aovisimap_cmd+"insert into uao_visimap_test05 select i,'aa'||i,i+10 from generate_series(1,5) as i;\ndelete from uao_visimap_test05 where i=3;\nselect * from uao_visimap_test05;\n"+aovisimap_cmd+"truncate table uao_visimap_test05;\nselect * from uao_visimap_test05;\n"+aovisimap_cmd
        with open(sql_file,'w') as f:
                f.write(sql_cmd3);
	sql_out=PSQL.run_sql_file(sql_file=sql_file, out_file=out_file,  flags='-q')
        assert Gpdiff.are_files_equal(out_file, ans_file)

    def test_06_deletetable_visimap_for_uao_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_06 Verify that the visimap updates with delete row in uao table test run')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'deletetablevisimapinfo_06.out')
        sql_file = os.path.join(self.sqlpath,'deletetablevisimapinfo_06.sql')
        ans_file= os.path.join(self.anspath,'deletetablevisimapinfo_06.ans')
        sql_cmd1="drop table if exists uao_visimap_test06 ;create table uao_visimap_test06 (i int, j varchar(20), k int ) with (appendonly=true) DISTRIBUTED BY (i);\n"
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
	self.assertIsNotNone(re.search('CREATE TABLE', sql_out))
        sql_cmd2="\\pset tuples_only\n\\pset footer off\nSELECT relfilenode FROM pg_class WHERE relname='uao_visimap_test06';\n"
        with open(sql_file,'w') as f:
                f.write(sql_cmd2);
        sql_out=PSQL.run_sql_file(sql_file=sql_file, out_file=out_file,  flags='-q')
        with open(out_file, 'r') as f:
            relid = f.read()
	aovisimap_cmd="select count(*) from gp_dist_random('pg_aoseg.pg_aovisimap_%s');\n" % relid.strip()
	sql_cmd3="select * from uao_visimap_test06;\n"+aovisimap_cmd+"insert into uao_visimap_test06 select i,'aa'||i,i+10 from generate_series(1,5) as i;\ndelete from uao_visimap_test06 where i=3;\nselect * from uao_visimap_test06;\n"+aovisimap_cmd
        with open(sql_file,'w') as f:
                f.write(sql_cmd3);
	sql_out=PSQL.run_sql_file(sql_file=sql_file, out_file=out_file,  flags='-q')
        assert Gpdiff.are_files_equal(out_file, ans_file)


    def test_08_call_udf_gp_aovisimap_fordelete(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_08 Verify the usage of UDF gp_aovisimap in utility mode for deleted tuple')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_aovisimap_del_08.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_aovisimap_del_08.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_aovisimap_del_08.ans')
	tablename='uao_visimap_test08'

	#create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
	assert Gpdiff.are_files_equal(out_file, ans_file)
	#get relid for newly created table
        relid = self.get_relid(file_name=tablename )
	#get the segment_id where we'll log in utility mode and then get the hostname and port for this segment
        utilitymodeinfo=self.get_utilitymode_conn_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

	before_tablerowcnt=self.get_rowcnt_table_on_segment(tablename=tablename, host=u_host,port=u_port)
	before_visimaprowcnt=self.get_visimap_cnt_on_segment(relid=relid,host=u_host,port=u_port)
	assert(int(before_visimaprowcnt) == 0)
	sql_cmd="delete from uao_visimap_test08 ;"
        PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=u_host, port=u_port,flags='-q -t')	
	after_tablerowcnt=self.get_rowcnt_table_on_segment(tablename=tablename, host=u_host,port=u_port)
	after_visimaprowcnt=self.get_visimap_cnt_on_segment(relid=relid,host=u_host,port=u_port)
	assert(int(after_tablerowcnt) == 0)

    def test_09_call_udf_gp_aovisimap_forupdate(self):
	tablename='uao_visimap_test09'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_09 Verify the usage of UDF gp_aovisimap in utility mode for update tuple')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_aovisimap_upd_09.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_aovisimap_upd_09.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_aovisimap_upd_09.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename)
	utilitymodeinfo=self.get_utilitymode_conn_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]
        #login to segment in utility mode and execute the gp_aovisimap(relid) UDF
        before_tablerowcnt=self.get_rowcnt_table_on_segment(tablename=tablename, host=u_host,port=u_port)
        before_visimaprowcnt=self.get_visimap_cnt_on_segment(relid=relid,host=u_host,port=u_port)
        assert(int(before_visimaprowcnt) == 0)
        sql_cmd="update %s set j = j || '_9';" % (tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=u_host, port=u_port,flags='-q -t')
        after_visimaprowcnt=self.get_visimap_cnt_on_segment(relid=relid,host=u_host,port=u_port)
        assert(int(after_visimaprowcnt) > 0)


    def test_10_use_udf_gp_aovisimap_hidden_info_uao_del_vacuum(self):
        tablename ='uao_table_test11'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_10 Verify the hidden tup_count using udf gp_aovisimap_hidden_info(oid)  for uao relation after delete and vacuum')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.sql')
        ans_file= os.path.join(self.anspath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )

        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
        #get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

        # delete  row
        sql_cmd3="delete from  %s  ;" % ( tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')
        assert(int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)) > 0)

        self.vacuum_full(tablename=tablename)
        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

    def test_11_tupcount_in_pg_aoseg_uaotable_upd(self):
	tablename ='uao_table_test11'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_11 Verify the tupcount update to pg_aoseg wile updating uao table ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.sql')
        ans_file= os.path.join(self.anspath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )


        #get the count from pg_aoseg before the update is performed
        pgaoseg_tupcount=self.get_tupcount_pgaoseg(relid=relid)
        tab_rowcount_bfrupd=self.get_rowcnt_table(tablename=tablename)

	sql_cmd3="update %s set j=j||'test11' where i = 1;" % tablename
        PSQL.run_sql_command(sql_cmd=sql_cmd3,  flags='-q -t')

        #get the count from pg_aoseg after  the update is performed
        pgaoseg_tupcount_aftupd=self.get_tupcount_pgaoseg(relid=relid)
        tab_rowcount_aftupd=self.get_rowcnt_table(tablename=tablename)

	assert (int(pgaoseg_tupcount_aftupd) == (int(tab_rowcount_aftupd))+1)
	assert (int(tab_rowcount_bfrupd) == (int(tab_rowcount_aftupd)))
	assert (int(pgaoseg_tupcount_aftupd) == (int(pgaoseg_tupcount))+1)
	# vacuum
	self.vacuum_full(tablename=tablename)
	
        pgaoseg_tupcount_aftvacuum=self.get_tupcount_pgaoseg(relid=relid)
        tab_rowcount_aftvacuum=self.get_rowcnt_table(tablename=tablename)
	assert (int(pgaoseg_tupcount_aftvacuum) == (int(tab_rowcount_aftvacuum)))


    def test_12_tupcount_in_pg_aoseg_uaotable_del(self):
        tablename ='uao_table_test12'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_12 Verify the tupcount delete to pg_aoseg with deleting from uao table ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_tupcount_in_pg_aoseg_uaotable_del_12.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_tupcount_in_pg_aoseg_uaotable_del_12.sql')
        ans_file= os.path.join(self.anspath,'create_tab_tupcount_in_pg_aoseg_uaotable_del_12.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )


        #get the count from pg_aoseg before the delete is performed
        pgaoseg_tupcount=self.get_tupcount_pgaoseg(relid=relid)
        tab_rowcount_bfrdel=self.get_rowcnt_table(tablename=tablename)

        sql_cmd3="delete from %s  where i = 1;" % tablename
        PSQL.run_sql_command(sql_cmd=sql_cmd3,  flags='-q -t')

        #get the count from pg_aoseg after the delete is performed
        pgaoseg_tupcount_aftdel=self.get_tupcount_pgaoseg(relid=relid)
        tab_rowcount_aftdel=self.get_rowcnt_table(tablename=tablename)

	assert (int(pgaoseg_tupcount_aftdel) == (int(tab_rowcount_aftdel))+1)
	assert (int(tab_rowcount_bfrdel) == (int(tab_rowcount_aftdel)+1))
	assert (int(pgaoseg_tupcount_aftdel) == (int(pgaoseg_tupcount)))

	# vacuum
	self.vacuum_full(tablename=tablename)
	
        pgaoseg_tupcount_aftvacuum=self.get_tupcount_pgaoseg(relid=relid)
        tab_rowcount_aftvacuum=self.get_rowcnt_table(tablename=tablename)
	assert (int(pgaoseg_tupcount_aftvacuum) == (int(tab_rowcount_aftvacuum)))

    def test_13_gp_persistent_relation_node_uaotable_del(self):
        tablename ='uao_table_test13'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_13 Verify the eof mark in pg_aoseg and gp_persistant_rel_node table for uao relation after delete ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_persistent_relation_node_uaotable_del_13.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_persistent_relation_node_uaotable_del_13.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_persistent_relation_node_uaotable_del_13.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
	#get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

        #get eof info before delete
        assert (self.is_same_eof_uao_on_segment(relid=relid,host=u_host,port= u_port))

	# delete 1 row
        sql_cmd3="delete from %s  where i = (select min(i) from %s );" % (tablename, tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')
        self.vacuum_full(tablename=tablename)

        #get eof info after delete
        #eof_pg_aoseg_aft=self.get_eofuncompressed_pgaoseg_on_segment(relid=relid,host=u_host,port= u_port)
        #eof_gp_persistant_rel_node_aft=self.get_eof_gp_persistent_relation_node_on_segment(relid=relid,host=u_host,port= u_port)
        #assert (int(eof_pg_aoseg_aft) == (int(eof_gp_persistant_rel_node_aft)))
        assert (self.is_same_eof_uao_on_segment(relid=relid,host=u_host,port= u_port))

    def test_14_gp_persistent_relation_node_uaotable_del(self):
        tablename ='uao_table_test14'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_14 Verify the eof mark in pg_aoseg and gp_persistant_rel_node table for uao relation after update ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_persistent_relation_node_uaotable_upd_14.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_persistent_relation_node_uaotable_upd_14.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_persistent_relation_node_uaotable_upd_14.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
	#get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

        #get eof info before update
	assert (self.is_same_eof_uao_on_segment(relid=relid,host=u_host,port= u_port))

	# update 1 row
        sql_cmd3="update %s set k = k+ 100 where i = (select min(i) from %s );" % (tablename, tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')
        self.vacuum_full(tablename=tablename)

        #get eof info after update
	assert (self.is_same_eof_uao_on_segment(relid=relid,host=u_host,port= u_port))

    def test_15_create_uaocs_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_15 Verify creation of uaocs table for the test run')
        tinctest.logger.info("-------------------------------\n")
        sql_file = os.path.join(self.sqlpath,'create_uaocs_table_01.sql')
        out_file = os.path.join(self.outpath,'create_uaocs_table_01.out')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        if out_file[-2:] == '.t':
            out_file = out_file[:-2]
        if self.anspath is not None:
            ans_file= os.path.join(self.anspath,'create_uaocs_table_01.ans')
            tinctest.logger.info("**** _run_sql_file() : ans file = "+ans_file)
            assert Gpdiff.are_files_equal(out_file, ans_file)
        os.remove(os.path.join('.', out_file))

    def test_16_check_empty_visimap_for_uaocs_tables(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_16 Verify Empty visimap for  uao table for the test run')
        tinctest.logger.info("-------------------------------\n")
        tablename="uaocs_tab_test1"
        relid = self.get_relid(file_name=tablename )
        sql_cmd="SELECT 1 FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=%s" % (relid)
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd)
        self.assertIsNotNone(re.search('1 row', sql_out))

    def test_17_gp_persistent_relation_node_uaocs_table_eof_del(self):
        tablename ='uaocs_table_test14'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_17 Verify the eof mark in pg_aoseg and gp_persistant_rel_node table for uaocs relation after delete ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.ans')
	#create uaocs table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
	#get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_uaocs_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]
        assert (self.is_same_eof_uaocs_on_segment(relid=relid,host=u_host,port= u_port))

	# update 1 row
        sql_cmd3="update %s set k = k+ 100 where i = (select min(i) from %s );" % (tablename, tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')
        self.vacuum()
        #self.vacuum(tablename=tablename)

        assert (self.is_same_eof_uaocs_on_segment(relid=relid,host=u_host,port= u_port))

    def test_18_gp_persistent_relation_node_uaocs_table_eof_upd(self):
        tablename ='uaocs_table_test14'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_18 Verify the eof mark in pg_aoseg and gp_persistant_rel_node table for uaocs relation after update ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.ans')
	#create uaocs table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
	#get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_uaocs_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

        assert (self.is_same_eof_uaocs_on_segment(relid=relid,host=u_host,port= u_port))

	# delete 1 row
        sql_cmd3="delete from  %s  where i = (select min(i) from %s );" % (tablename, tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')
        self.vacuum_full(tablename=tablename)

        assert (self.is_same_eof_uaocs_on_segment(relid=relid,host=u_host,port= u_port))


    def test_20_use_udf_gp_aovisimap_hidden_info_uaocs_del(self):
        tablename ='uaocs_table_test14'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_20 Verify the hidden tup_count using UDF gp_aovisimap_hidden_info(oid)  for uaocs relation after delete ')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.sql')
        ans_file= os.path.join(self.anspath,'create_tab_gp_persistent_relation_node_uaocs_table_upd_14.ans')
        #create uaocs table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
        #get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_uaocs_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

        # delete 1 row
        sql_cmd3="delete from  %s  where i = (select min(i) from %s );" % (tablename, tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')

        assert(1 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

        self.vacuum_full(tablename=tablename)
        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

    def test_21_use_udf_gp_aovisimap_hidden_info_uao_upd_vacuum(self):
        tablename ='uao_table_test11'
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('test_21 Verify the hidden tup_count using UDF gp_aovisimap_hidden_info(oid)  for uao relation after update_vacuum')
        tinctest.logger.info("-------------------------------\n")
        out_file = os.path.join(self.outpath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.out')
        sql_file = os.path.join(self.sqlpath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.sql')
        ans_file= os.path.join(self.anspath,'create_tab_tupcount_in_pg_aoseg_uaotable_upd_11.ans')

        #create uao table and insert 10 rows
        sql_out=PSQL.run_sql_file(sql_file = sql_file,out_file=out_file)
        assert Gpdiff.are_files_equal(out_file, ans_file)
        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )

        #get relid for newly created table
        relid = self.get_relid(file_name=tablename )
        #get utility mode connection info
        utilitymodeinfo=self.get_utilitymode_conn_info( relid=relid)
        u_port=utilitymodeinfo[0]
        u_host=utilitymodeinfo[1]

        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))

        # update  rows
        sql_cmd3="update %s set j = 'test11' ;" % ( tablename)
        PSQL.run_sql_command_utility_mode(sql_cmd= sql_cmd3,host=u_host, port=u_port,flags='-q -t')

        assert(int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)) > 0)

        self.vacuum_full(tablename=tablename)
        assert(0 == int(self.get_hidden_tup_cnt(relid=relid,host=u_host,port= u_port)))





    def get_rowcnt_table(self,tablename=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting rowcount from table '+tablename)
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select count(*) from %s;" % tablename
        rowcount=PSQL.run_sql_command(sql_cmd=sql_cmd,flags='-q -t')
        return rowcount

    def get_rowcnt_table_on_segment(self,tablename=None, host=None,port=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting rowcount from table '+tablename +' on segment host='+host +' port=' + port)
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select count(*) from %s;" % tablename
        rowcount= PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=host, port=port,flags='-q -t')
        return rowcount

    def get_visimap_cnt_on_segment(self,relid=0,host=None,port=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting rowcount from relid '+ relid +' on segment host='+host +' port=' + port)
        tinctest.logger.info("-------------------------------\n")
        sql_cmd="select count(*) from gp_aovisimap(%s);" % relid.strip()
        rowcount= PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=host, port=port,flags='-q -t')
        return rowcount
	

    def get_tupcount_pgaoseg(self,relid=0):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting tupcount from pg_aoseg.pg_aoseg_ '+relid)
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select sum(tupcount)  from gp_dist_random('pg_aoseg.pg_aoseg_%s');" % relid.strip()
	tupcount=PSQL.run_sql_command(sql_cmd=sql_cmd,flags='-q -t')
	return tupcount

    def get_tupcount_pgaocsseg(self,relid=0):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting tupcount from pg_aoseg.pg_aocsseg_ '+relid)
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select sum(tupcount)  from gp_dist_random('pg_aoseg.pg_aocsseg_%s');" % relid.strip()
	tupcount=PSQL.run_sql_command(sql_cmd=sql_cmd,flags='-q -t')
	return tupcount

    def get_eofuncompressed_pgaoseg_on_segment(self,relid=0,host=None,port=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting eofuncompressed from pg_aoseg.pg_aoseg_ '+relid)
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select eofuncompressed  from pg_aoseg.pg_aoseg_%s where eof > 0 LIMIT 1;" % relid.strip()
	eof=PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=host, port=port,flags='-q -t')
	return eof


    def is_same_eof_uaocs_on_segment(self,relid=0,host=None,port=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting eofuncompressed from gp_persistent_relation_node and udf get_gp_aocsseg(oid) ')
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select count(*) from gp_persistent_relation_node a , get_gp_aocsseg(%s) b  where a.segment_file_num = b.physical_segno and relfilenode_oid = %s and mirror_append_only_new_eof != eof;" %(relid,relid)
	cnt=PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=host, port=port,flags='-q -t')
	if (int(cnt) == 0):
		return True
	else:
		return False

    def is_same_eof_uao_on_segment(self,relid=0,host=None,port=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('getting eofuncompressed from gp_persistent_relation_node and pg_aoseg.pg_aoseg_oid ')
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select count(*) from gp_persistent_relation_node a , pg_aoseg.pg_aoseg_%s b where a.segment_file_num = b.segno and relfilenode_oid = %s and mirror_append_only_new_eof != eofuncompressed;" %(relid.strip(),relid.strip())
	cnt=PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=host, port=port,flags='-q -t')
	if (int(cnt) == 0):
		return True
	else:
		return False


    def create_udf_get_gp_aocsseg(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('creating UDF get_gp_aocsseg(oid)')
        tinctest.logger.info("-------------------------------\n")
        sql_cmd1="drop function if exists get_gp_aocsseg(oid);  CREATE FUNCTION get_gp_aocsseg(oid) RETURNS TABLE ( gp_tid tid, segno integer, column_num smallint, physical_segno integer, tupcount bigint, eof bigint, eof_uncompressed bigint, modcount bigint, formatversion smallint, ownstate smallint ) AS '"+self.gphome+"/lib/postgresql/gp_ao_co_diagnostics.so', 'gp_aocsseg_wrapper' LANGUAGE C STRICT;"
        tinctest.logger.info(sql_cmd1+"\n")
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
        if (re.search('CREATE FUNCTION', sql_out)) is not None:
        	tinctest.logger.info('Successfully created UDF get_gp_aocsseg(oid) ') 
        self.assertIsNotNone(re.search('CREATE FUNCTION', sql_out))

    def create_udf_gp_aovisimap(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('creating UDF gp_aovisimap ')
        tinctest.logger.info("-------------------------------\n")
        sql_cmd1="drop function if exists  gp_aovisimap(oid); create function gp_aovisimap(oid) RETURNS TABLE (tid tid, segno integer, row_num bigint)  AS '"+self.gphome+"/lib/postgresql/gp_ao_co_diagnostics.so', 'gp_aovisimap_wrapper' LANGUAGE C STRICT;"
        tinctest.logger.info(sql_cmd1+"\n")
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
        if (re.search('CREATE FUNCTION', sql_out)) is not None:
        	tinctest.logger.info('Successfully created UDF gp_aovisimap(oid) ') 
        self.assertIsNotNone(re.search('CREATE FUNCTION', sql_out))


    def create_udf_gp_aovisimap_hidden_info(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('creating UDF gp_aovisimap_hidden_info ')
        tinctest.logger.info("-------------------------------\n")
        sql_cmd1="drop function if exists  gp_aovisimap_hidden_info(oid); create function gp_aovisimap_hidden_info(oid) RETURNS TABLE   (segno integer, hidden_tupcount bigint, total_tupcount bigint)    AS '"+self.gphome+"/lib/postgresql/gp_ao_co_diagnostics.so', 'gp_aovisimap_hidden_info_wrapper' LANGUAGE C STRICT;"
        tinctest.logger.info(sql_cmd1+"\n")
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
        if (re.search('CREATE FUNCTION', sql_out)) is not None:
        	tinctest.logger.info('Successfully created UDF gp_aovisimap_hidden_info(oid)  ') 
        self.assertIsNotNone(re.search('CREATE FUNCTION', sql_out))

    def get_hidden_tup_cnt(self, relid=0,host=None,port=None):
	tinctest.logger.info("-------------------------------")
        tinctest.logger.info('get hidden_tupcount from gp_aovisimap_hidden_info ')
        tinctest.logger.info("-------------------------------\n")
	sql_cmd="select sum(hidden_tupcount) from gp_aovisimap_hidden_info(%s);" % relid.strip()
	tupcnt=PSQL.run_sql_command_utility_mode(sql_cmd=sql_cmd,host=host, port=port,flags='-q -t')
	if (len(tupcnt.strip()) == 0):
		tupcnt='0'
	return tupcnt
	


    def vacuum_full(self, tablename=None):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('creating vacuum full '+tablename)
        tinctest.logger.info("-------------------------------\n")
        sql_cmd1="vacuum full %s;" % tablename
        tinctest.logger.info(sql_cmd1+"\n")
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
        if (re.search('VACUUM', sql_out)) is not None:
        	tinctest.logger.info('Successfully executed vacuum full on table ' + tablename ) 
        self.assertIsNotNone(re.search('VACUUM', sql_out))

    def vacuum(self):
        tinctest.logger.info("-------------------------------")
        tinctest.logger.info('creating vacuum')
        tinctest.logger.info("-------------------------------\n")
        sql_cmd1="vacuum;" 
        tinctest.logger.info(sql_cmd1+"\n")
        sql_out=PSQL.run_sql_command(sql_cmd = sql_cmd1)
        if (re.search('VACUUM', sql_out)) is not None:
        	tinctest.logger.info('Successfully executed vacuum ') 

    def get_relid(self,file_name=None):
	sql_cmd="SELECT oid FROM pg_class WHERE relname='%s';\n" % file_name
	relid= PSQL.run_sql_command(sql_cmd=sql_cmd,  flags='-q -t')
	return relid; 


	

    def get_utilitymode_conn_info(self, relid=0):
        #get the segment_id where to log in utility mode and then get the hostname and port for this segment
        sql_cmd="select port, hostname from gp_segment_configuration sc  where dbid > 1 and role = 'p' order by port desc limit 1;" 
        utilitymodeinfo=PSQL.run_sql_command(sql_cmd=sql_cmd,  flags='-q -t')
        u_port=utilitymodeinfo.strip().split('|')[0]
        u_host=utilitymodeinfo.strip().split('|')[1]
	return [u_port,u_host]

    def get_utilitymode_conn_uaocs_info(self, relid=0):
        #get the segment_id where to log in utility mode and then get the hostname and port for this segment
	sql_cmd="select port, hostname from gp_segment_configuration sc  where dbid > 1 and role = 'p' limit 1;"
        utilitymodeinfo=PSQL.run_sql_command(sql_cmd=sql_cmd,  flags='-q -t')
        u_port=utilitymodeinfo.strip().split('|')[0]
        u_host=utilitymodeinfo.strip().split('|')[1]
	return [u_port,u_host]

