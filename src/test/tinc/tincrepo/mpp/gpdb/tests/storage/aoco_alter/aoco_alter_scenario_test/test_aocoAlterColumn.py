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

import os
import glob
import sys
import tinctest
from tinctest.lib import local_path, Gpdiff
from mpp.models import MPPTestCase
from tinctest.models.scenario import ScenarioTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.aoco_alter.aoco_alter_scenario_test.aocoAlterColumn import AOCOAlterColumn


class AOCSAlterColumnTestCase(ScenarioTestCase, MPPTestCase):
    ''' Testing the performance enhancement related changes done to alter table add/drop column to aoco tables '''
    
    def __init__(self, methodName):
        super(AOCSAlterColumnTestCase,self).__init__(methodName)
        self.aoco_alt_obj=AOCOAlterColumn()

    @classmethod
    def setUpClass(cls):
        super(AOCSAlterColumnTestCase, cls).setUpClass()
        '''
        Create tables with multiple varblocks and multiple segfiles upfront. They will be used in the tests .
        '''
        base_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        crtable_name = ['create_tabfor_utility_mode','create_multivarblock_table','create_multisegfile_table','create_large_table']
        for sname in crtable_name:
            aoco_alter_sql=''
            aoco_alter_sql= os.path.join(os.path.dirname(sys.modules[cls.__module__].__file__), "sql")+'/'+sname+'.sql'
            aoco_alter_ans= os.path.join(os.path.dirname(sys.modules[cls.__module__].__file__), "expected")+'/'+sname+'.ans'
            aoco_alter_out= os.path.join(os.path.dirname(sys.modules[cls.__module__].__file__), "sql")+'/'+sname+'.out'
            tinctest.logger.info( '\n Creating TABLE :  %s' % aoco_alter_sql )
            res=PSQL.run_sql_file(sql_file = aoco_alter_sql,out_file=aoco_alter_out)
            init_file=os.path.join( base_dir, "sql",'init_file')
            result = Gpdiff.are_files_equal(aoco_alter_out, aoco_alter_ans, match_sub =[init_file])
            errmsg='Gpdiff are not equal for file '+ sname
            assert result, errmsg

    def test_AOCOAlterColumnChangeTracking_05_addcolumn_largetable(self):
        ''' 
        @product_version gpdb: [4.3.2.0-]
        
        This tests the behavior of alter table in a failover/changtracking scenario 
        STEPS:
        1. Alter table add column
        2. While the alter is going on, Fail one Primary
        3. Make sure the trasaction in step 1 is aborted
        4. gprecoverseg 
        5. Alter table add column again 
        6. Make sure transaction in step 5 passes
        '''
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting Test: test_AOCOAlterColumnChangeTracking_05_addcolumn_largetable ") 
        tinctest.logger.info("\n ===============================================")
        self.aoco_alt_obj.run_test_ChangeTracking('largetab-altercol')


    def test_NegativeAOCOAlter_UtilityMode_06_addcolumn_utilityMode(self):
        ''' 
        @product_version gpdb: [4.3.2.0-]
        
        This tests the behavior of alter table in a utility mode connection
        STEPS:
        1. Make a Utlity Mode connection
        2. Alter table add column
        3. Make sure the alter fails
        '''
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting Test: test_NegativeAOCOAlter_UtilityMode_06_addcolumn_utilityMode") 
        tinctest.logger.info("\n ===============================================")
        self.aoco_alt_obj.run_test_utility_mode('alter_aoco_tab_utilitymode')




    def test_AOCOAlterColumnCatalogCheck(self):
        ''' 
        @product_version gpdb: [4.3.2.0-]
        
        These set of tests verify the correctness of catalog after alter table add/drop column
        involving multi-varblock and multi-segfile Appendonly column oriented table
        STEPS:
        1. perform alter table add/drop column to multi-varblock/multi-segfile
        2. Verify the correctness of the operation
        3. Use gpcheckcat to validate the correctness of catalog after alter table operation

        @data_provider data_types_provider
        '''
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting Test: %s%s " % (self.test_data[0][0], self.test_data[0][1] ))
        action = self.test_data[1][0]
        storage= self.test_data[1][1]
        tinctest.logger.info("\n SQL Action   : %s" %(action)) 
        tinctest.logger.info("\n Storage type : %s" %(storage)) 
        tinctest.logger.info("\n ===============================================")
        self.aoco_alt_obj.run_test_CatalogCheck(action,storage)
        self.aoco_alt_obj.validate_test_CatalogCheck(action,storage)

@tinctest.dataProvider('data_types_provider')

def test_data_provider():
    data = { 
             '01_addcol_multisegfiles': ['addcol','multisegfiles']
             ,'02_dropcol_multisegfiles': ['dropcol','multisegfiles']
             ,'03_addcol_multivarblock': ['addcol','multivarblock']
             ,'04_dropcol_multivarblock': ['dropcol','multivarblock']
            }
    return data



