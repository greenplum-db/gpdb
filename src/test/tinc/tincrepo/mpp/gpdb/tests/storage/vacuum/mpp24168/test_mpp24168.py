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
from mpp.models import MPPTestCase
import time

from tinctest.lib import local_path, run_shell_command
from mpp.lib.PSQL import PSQL


class RelfrozenxidUpdateTestCase(MPPTestCase):
    '''
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3.3-]
    '''
    #These tests verify if vacuum freeze correctly updates age of auxiliary tables along with the age of parent appendonly table

    def run_validation(self):
        
        relid_out = PSQL.run_sql_command("select oid from pg_class where relname='test_table';", flags ='-q -t')
        relid_out = relid_out.replace("\n", "").strip()

        #doing multiple selects to increase the age of the table
        for x in range(0, 150):
            PSQL.run_sql_command("select * from test_table", flags ='-q -t')


        PSQL.run_sql_command("drop table if exists table_master; \
                              create table table_master as select age(relfrozenxid), relname \
                              from pg_class \
                              where relkind in ('r','t','o','b','m') and relstorage not in ('x','f','v') and (relname like '%"+str(relid_out)+"%' or relname ='test_table' )", \
                              flags ='-q -t')
        PSQL.run_sql_command("drop table if exists table_seg; \
                              create table table_seg as select age(relfrozenxid), relname \
                              from gp_dist_random('pg_class') \
                              where relkind in ('r','t','o','b','m') and relstorage not in ('x','f','v') and (relname like '%"+str(relid_out)+"%' or relname ='test_table' )", \
                              flags ='-q -t', out_file="debug.out")

        count=PSQL.run_sql_command("select count(*) from table_master where age > 30", flags ='-q -t')
        count=count.replace("\n", "").strip()
        if int(count)== 0: 
            timestr = time.strftime("ages_from_master_%Y%m%d-%H%M%S.out")
            out_file=local_path(timestr)
            PSQL.run_sql_command("select *  from table_master", flags ='-q -t', out_file=out_file)
            self.fail("the age was not increased correctly. Refer %s" %out_file)

        count=PSQL.run_sql_command("select count(*) from table_seg where age > 30", flags ='-q -t')
        count=count.replace("\n", "").strip()
        if int(count) == 0: 
            timestr = time.strftime("ages_from_seg_%Y%m%d-%H%M%S.out")
            out_file=local_path(timestr)
            PSQL.run_sql_command("select * from table_seg", flags ='-q -t', out_file=out_file)
            self.fail("the age was not increased correctly. Refer %s" %out_file)


        PSQL.run_sql_command("set vacuum_freeze_min_age = 30;", flags ='-q -t')
        PSQL.run_sql_command("vacuum freeze test_table;", flags ='-q -t')

        PSQL.run_sql_command("drop table if exists table_master; \
                              create table table_master as select age(relfrozenxid), relname \
                              from pg_class \
                              where relkind in ('r','t','o','b','m') and relstorage not in ('x','f','v') and (relname like '%"+str(relid_out)+"%' or relname ='test_table' )", \
                              flags ='-q -t')
        PSQL.run_sql_command("drop table if exists table_seg; \
                              create table table_seg as select age(relfrozenxid), relname \
                              from gp_dist_random('pg_class') \
                              where relkind in ('r','t','o','b','m') and relstorage not in ('x','f','v') and (relname like '%"+str(relid_out)+"%' or relname ='test_table' )", \
                              flags ='-q -t', out_file="debug.out")

        count=PSQL.run_sql_command("select count(*) from table_master where age > 30", flags ='-q -t')
        count=count.replace("\n", "").strip()
        if not(int(count)== 0):
            timestr = time.strftime("ages_from_master_%Y%m%d-%H%M%S.out")
            out_file=local_path(timestr)
            PSQL.run_sql_command("select *  from table_master", flags ='-q -t', out_file=out_file)
            self.fail("vacuum did not happend correctly. Refer %s" %out_file)

        count=PSQL.run_sql_command("select count(*) from table_seg where age > 30", flags ='-q -t')
        count=count.replace("\n", "").strip()
        if not(int(count) == 0):
            timestr = time.strftime("ages_from_seg_%Y%m%d-%H%M%S.out")
            out_file=local_path(timestr)
            PSQL.run_sql_command("select * from table_seg", flags ='-q -t', out_file=out_file)
            self.fail("vacuum did not happend correctly. Refer %s" %out_file)

    def test_ao_notoast_empty(self):
        PSQL.run_sql_file(local_path("workload_ao_notoast_empty.sql"), out_file=local_path("workload_ao_notoast_empty.out"))    
        self.run_validation()
        #We perform insert, delete and vacuum before final validation to ensure that the table is empty again before validation
        PSQL.run_sql_command("INSERT INTO test_table select i, i*2, i*5 from generate_series(1, 20)i;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("update_ao_notoast_empty.out"))
        PSQL.run_sql_command("delete from test_table;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("delete_ao_notoast_empty.out"))
        PSQL.run_sql_command("VACUUM;", flags ='-q -t', out_file=local_path("vac_ao_notoast_empty.out"))

        self.run_validation()
         
         
         
    def test_ao_toast_empty(self):
        PSQL.run_sql_file(local_path("workload_ao_toast_empty.sql"), out_file=local_path("workload_ao_toast_empty.out"))    
        self.run_validation()
        PSQL.run_sql_command("INSERT INTO test_table select i, i*2, i*5, CURRENT_TIMESTAMP  from generate_series(1, 20)i;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("update_ao_toast_empty.out"))
        PSQL.run_sql_command("delete from test_table;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("delete_ao_toast_empty.out"))
        PSQL.run_sql_command("VACUUM;", flags ='-q -t', out_file=local_path("vac_ao_toast_empty.out"))
        self.run_validation()


    def test_ao_toast_update(self):
        PSQL.run_sql_file(local_path("workload_ao_toast_update.sql"), out_file=local_path("workload_ao_toast_update.out"))    
        self.run_validation()
        PSQL.run_sql_command("INSERT INTO test_table select i, i*2, i*5, CURRENT_TIMESTAMP  from generate_series(1, 20)i;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("update_ao_toast_update.out"))
        PSQL.run_sql_command("delete from test_table;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("delete_ao_toast_update.out"))
        PSQL.run_sql_command("VACUUM;", flags ='-q -t', out_file=local_path("vac_ao_toast_update.out"))

        self.run_validation()


    def test_co_notoast_empty(self):
        PSQL.run_sql_file(local_path("workload_co_notoast_empty.sql"), out_file=local_path("workload_co_notoast_empty.out"))    
        self.run_validation()
        PSQL.run_sql_command("INSERT INTO test_table select i, i*2, i*5 from generate_series(1, 20)i;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("update_co_notoast_empty.out"))
        PSQL.run_sql_command("delete from test_table;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("delete_co_notoast_empty.out"))
        PSQL.run_sql_command("VACUUM;", flags ='-q -t', out_file=local_path("vac_co_notoast_empty.out"))

        self.run_validation()


    def test_co_toast_empty(self):
        PSQL.run_sql_file(local_path("workload_co_toast_empty.sql"), out_file=local_path("workload_co_toast_empty.out"))    
        self.run_validation()
        PSQL.run_sql_command("INSERT INTO test_table select i, i*2, i*5, CURRENT_TIMESTAMP  from generate_series(1, 20)i;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("update_co_toast_empty.out"))
        PSQL.run_sql_command("delete from test_table;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("delete_co_toast_empty.out"))
        PSQL.run_sql_command("VACUUM;", flags ='-q -t', out_file=local_path("vac_co_toast_empty.out"))

        self.run_validation()

    def test_co_toast_update(self):
        PSQL.run_sql_file(local_path("workload_co_toast_update.sql"), out_file=local_path("workload_co_toast_update.out"))    
        self.run_validation()
        PSQL.run_sql_command("INSERT INTO test_table select i, i*2, i*5, CURRENT_TIMESTAMP from generate_series(1, 20)i;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("update_co_toast_update.out"))
        PSQL.run_sql_command("delete from test_table;"
                             "select count(*) from test_table;", flags ='-q -t', out_file=local_path("delete_co_toast_update.out"))
        PSQL.run_sql_command("VACUUM;", flags ='-q -t', out_file=local_path("vac_co_toast_update.out"))

        self.run_validation()



