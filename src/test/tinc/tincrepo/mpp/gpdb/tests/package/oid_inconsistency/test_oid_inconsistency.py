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

import os, re
import tinctest
import unittest2 as unittest
from tinctest.lib import Gpdiff
from tinctest.lib import local_path, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.gppkg.gppkg import Gppkg
from mpp.lib.PSQL import PSQL

GPHOME = os.environ.get("GPHOME")
LIBDIR = '%s/lib/postgresql' % (GPHOME)

class OidInconsistencyMPPTestCase(MPPTestCase):

    def __init__(self, methodName):
        self.dbname = os.environ.get('PGDATABASE')
        super(OidInconsistencyMPPTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(cls):
        """
        Checking if plperl package installed, otherwise install the package
        """
        super(OidInconsistencyMPPTestCase, cls).setUpClass()
        cmd = 'gpssh --version'
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (cmd, 'check product version', res)
        gppkg = Gppkg()
        product_version = res['stdout']
        gppkg.gppkg_install(product_version, 'plperl')


    def compare_oids(self, out_file):
        '''Get the oids from out file and check if all are same '''
        myfile = open(out_file, 'r')
        oid_ls = []
        for line in myfile:
             oid = re.match('\s*\d+', line) 
             if oid :
                oid_ls.append (oid.group(0)) 
        if len(set(oid_ls)) == 1:
           tinctest.logger.info("Success: Oids are consistent")
           return True
        else:
           tinctest.logger.info("Failure: Oids are not consistent, please refer %s " % out_file)
           return False


    def doTest(self, sql_filename):
        '''Run the file, compare oids in out file '''
        sql_file = local_path(sql_filename)
        out_file = local_path(sql_filename.split('.sql')[0] + '.out')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        isOk = self.compare_oids(out_file)
        self.assertTrue(isOk)
 
    def checkAPPHOMEandLIB(self, libso, apphome=''):
        if apphome == '':
           """ check application library """
           #print "%s/%s.so" % (LIBDIR, libso)
           return os.path.isfile("%s/%s.so" % (LIBDIR, libso))
        else:
           """ check application home and library """
           return os.environ.get(apphome) and os.path.isfile("%s/%s.so" % (LIBDIR, libso))


    def test_01_pg_language_oid(self):
        '''Oid_inconsistency : pg_langauge -oid  '''
        if self.checkAPPHOMEandLIB("plperl"):
            self.doTest('pg_language_oid.sql')
        else:
            self.skipTest('skipping test: no plperl.so found in $GPHOME/lib/postgresql')

    
    def test_02_pg_language_lanvalidator(self):
        '''Oid_inconsistency : pg_langauge -lanvalidator '''
        if self.checkAPPHOMEandLIB("plperl"):
            self.doTest('pg_language_lanvalidator.sql')
        else:
            self.skipTest('skipping test: no plperl.so found in $GPHOME/lib/postgresql')

    def test_03_pg_language_lanplcallfoid(self):
        '''Oid_inconsistency : pg_langauge -lanplcallfoid '''
        
        if self.checkAPPHOMEandLIB("plperl"):
            self.doTest('pg_language_lanplcallfoid.sql')
        else:
            self.skipTest('skipping test: no plperl.so found in $GPHOME/lib/postgresql')

    def test_04_pg_proc_oid(self):
        '''Oid_inconsistency : pg_proc -oid '''
        
        if self.checkAPPHOMEandLIB("plperl"):
            self.doTest('pg_proc_oid_1.sql')
            self.doTest('pg_proc_oid_2.sql')
        else:
            self.skipTest('skipping test: no plperl.so found in $GPHOME/lib/postgresql')

    def test_05_pg_rewrite_view(self):
        '''Oid_inconsistency : pg_rewrite create view -oid '''
        self.doTest('pg_rewrite_view.sql')

    def test_06_pg_rewrite_rule_select(self):
        '''Oid_inconsistency : pg_rewrite_rule select event -oid '''
        self.doTest('pg_rewrite_rule_select.sql')

    def test_07_pg_rewrite_rule_update(self):
        '''Oid_inconsistency : pg_rewrite_rule update event -oid '''
        self.doTest('pg_rewrite_rule_update.sql')

    def test_08_pg_rewrite_rule_delete(self):
        '''Oid_inconsistency : pg_rewrite_rule delete event -oid '''
        self.doTest('pg_rewrite_rule_delete.sql')

    def test_09_pg_rewrite_rule_insert(self):
        '''Oid_inconsistency : pg_rewrite_rule insert event -oid '''
        self.doTest('pg_rewrite_rule_insert.sql')

    def test_10_pg_trigger_oid(self):
        '''Oid_inconsistency : pg_trigger -oid '''
        self.doTest('pg_trigger_oid.sql')

    def test_11_pg_constraint_create_table_column_unique(self):
        '''Oid_inconsistency : pg_constaint_create table column constraint_unique -oid,conrelid '''
        self.doTest('pg_constraint_create_table_column_unique.sql')
        self.doTest('pg_constraint_create_table_column_unique_2.sql')
   
    def test_12_pg_constraint_create_table_column_primary_key(self):
        '''Oid_inconsistency : pg_constaint_create table column constraint primary_key -oid,conrelid '''
        self.doTest('pg_constraint_create_table_column_primary_key.sql')
        self.doTest('pg_constraint_create_table_column_primary_key_2.sql')

    def test_13_pg_constraint_create_table_column_check(self):
        '''Oid_inconsistency : pg_constaint_create table column constraint check -oid,conrelid '''
        self.doTest('pg_constraint_create_table_column_check.sql')
        self.doTest('pg_constraint_create_table_column_check_2.sql')

    def test_14_pg_constraint_create_table_unique(self):
        '''Oid_inconsistency : pg_constaint_create table_unique -oid,conrelid '''
        self.doTest('pg_constraint_create_table_unique.sql')
        self.doTest('pg_constraint_create_table_unique_2.sql')
   
    def test_15_pg_constraint_create_table_primary_key(self):
        '''Oid_inconsistency : pg_constaint_create table_primary_key -oid,conrelid '''
        self.doTest('pg_constraint_create_table_primary_key.sql')
        self.doTest('pg_constraint_create_table_primary_key_2.sql')

    def test_16_pg_constraint_create_table_check(self):
        '''Oid_inconsistency : pg_constaint_create table_check -oid,conrelid '''
        self.doTest('pg_constraint_create_table_check.sql')
        self.doTest('pg_constraint_create_table_check_2.sql')

    def test_17_pg_constraint_create_table_like(self):
        '''Oid_inconsistency : pg_constaint_create table like -oid,conrelid '''
        self.doTest('pg_constraint_create_table_like.sql')
        self.doTest('pg_constraint_create_table_like_2.sql')

    def test_18_pg_constraint_create_table_inherit(self):
        '''Oid_inconsistency : pg_constaint_create table inherit -oid,conrelid '''
        self.doTest('pg_constraint_create_table_inherit.sql')
        self.doTest('pg_constraint_create_table_inherit_2.sql')

    def test_19_pg_constraint_create_table_partition(self):
        '''Oid_inconsistency : pg_constaint create partition table -oid'''
        self.doTest('pg_constraint_create_table_partition.sql')
        self.doTest('pg_constraint_create_table_partition_2.sql')
        self.doTest('pg_constraint_create_table_partition_3.sql')
        self.doTest('pg_constraint_create_table_partition_4.sql')

    def test_20_pg_constraint_create_table_partition_unique(self):
        '''Oid_inconsistency : pg_constaint create partition table with unique  -oid,conrelid'''
        self.doTest('pg_constraint_create_table_partition_unique.sql')
        self.doTest('pg_constraint_create_table_partition_unique_2.sql')

    def test_21_pg_constraint_alter_table_add_primary_key(self):
        '''Oid_inconsistency : pg_constaint_alter_table_add_primary_key -oid,conrelid '''
        self.doTest('pg_constraint_alter_table_add_primary_key.sql')
        self.doTest('pg_constraint_alter_table_add_primary_key_2.sql')

    def test_22_pg_constraint_alter_table_add_unique(self):
        '''Oid_inconsistency : pg_constaint_alter_table_add_unique -oid,conrelid '''
        self.doTest('pg_constraint_alter_table_add_unique.sql')
        self.doTest('pg_constraint_alter_table_add_unique_2.sql')

    def test_23_pg_constraint_alter_table_add_check(self):
        '''Oid_inconsistency : pg_constaint_alter_table_add_check -oid,conrelid '''
        self.doTest('pg_constraint_alter_table_add_check.sql')
        self.doTest('pg_constraint_alter_table_add_check_2.sql')

    def test_24_pg_constraint_alter_table_add_column_constraint(self):
        '''Oid_inconsistency : pg_constaint_alter_table_add_column_with_constraint -oid,conrelid '''
        self.doTest('pg_constraint_alter_table_add_column_constraint.sql')
        self.doTest('pg_constraint_alter_table_add_column_constraint_2.sql')
    
    def test_25_pg_constraint_alter_part_table_add_column_constraint(self):
        '''Oid_inconsistency : pg_constaint_alter_part_table_add_column_with_constraint -oid,conrelid '''
        self.doTest('pg_constraint_alter_part_table_add_column_constraint.sql')
        self.doTest('pg_constraint_alter_part_table_add_column_constraint_2.sql')
        self.doTest('pg_constraint_alter_part_table_add_column_constraint_3.sql')

    def test_26_pg_constraint_alter_table_add_partition(self):
        '''Oid_inconsistency : pg_constaint_alter_table_add_partition -oid'''
        self.doTest('pg_constraint_alter_table_add_partition.sql')
    
    # Commenting due to MPP-13685
    #def test_27_pg_constraint_alter_table_exchange_partition(self):
    #    '''Oid_inconsistency : pg_constaint_alter_table_exchange_partition -oid'''
    #    self.doTest('pg_constraint_alter_table_exchange_partition.sql')
    
    def test_28_pg_constraint_alter_table_split_partition(self):
        '''Oid_inconsistency : pg_constaint_alter_table_split_partition -oid'''
        self.doTest('pg_constraint_alter_table_split_partition.sql')
        self.doTest('pg_constraint_alter_table_split_partition_2.sql')

    def test_29_pg_constraint_create_domain(self):
        '''Oid_inconsistency : pg_constaint_create_domain -oid'''
        self.doTest('pg_constraint_create_domain.sql')

    def test_30_pg_constraint_alter_domain(self):
        '''Oid_inconsistency : pg_constaint_alter_domain -oid'''
        self.doTest('pg_constraint_alter_domain.sql')

    def test_31_pg_constraint_create_table_foreign_key(self):
        '''Oid_inconsistency : pg_constaint create_table_foreign_key -oid'''
        self.doTest('pg_constraint_create_table_foreign_key.sql')
        self.doTest('pg_constraint_create_table_foreign_key_2.sql')

    def test_32_pg_constraint_alter_table_foreign_key(self):
        '''Oid_inconsistency : pg_constaint alter_table_foreign_key -oid'''
        self.doTest('pg_constraint_alter_table_foreign_key.sql')
        self.doTest('pg_constraint_alter_table_foreign_key_2.sql')

    def test_33_pg_constraint_alter_table_inherits_add_constraint(self):
        '''Oid_inconsistency : pg_constaint_alter_table_inherits_add_constraint -oid'''
        self.doTest('pg_constraint_alter_table_inherits_add_constraint.sql')
        self.doTest('pg_constraint_alter_table_inherits_add_constraint_2.sql')

    def test_34_pg_constraint_alter_table_alter_type(self):
        '''Oid_inconsistency : pg_constaint_alter_table_alter data type -oid,conrelid '''
        self.doTest('pg_constraint_alter_table_alter_type.sql')
        self.doTest('pg_constraint_alter_table_alter_type_2.sql')

    def test_35_pg_constraint_create_table_partition_primary(self):
        '''Oid_inconsistency : pg_constaint create partition table with primary key  -oid,conrelid'''
        self.doTest('pg_constraint_create_table_partition_primary.sql')
        self.doTest('pg_constraint_create_table_partition_primary_2.sql')
     
    # Commenting due to MPP-14089
    #def test_36_pg_constraint_create_part_table_foreign_key(self):
    #    '''Oid_inconsistency : pg_constaint create_part_table_foreign_key -oid'''
    #    self.doTest('pg_constraint_create_part_table_foreign_key.sql')
    #    self.doTest('pg_constraint_create_part_table_foreign_key_2.sql')
    #    self.doTest('pg_constraint_create_part_table_foreign_key_3.sql')
    #    self.doTest('pg_constraint_create_part_table_foreign_key_4.sql')
    
    #def test_37_pg_constraint_alter_part_table_add_foreign_key(self):
    #    '''Oid_inconsistency : pg_constaint alter_part_table_add_foreign_key -oid'''
    #    self.doTest('pg_constraint_alter_part_table_add_foreign_key.sql')
    #    self.doTest('pg_constraint_alter_part_table_add_foreign_key_2.sql')
    #    self.doTest('pg_constraint_alter_part_table_add_foreign_key_3.sql')
    #    self.doTest('pg_constraint_alter_part_table_add_foreign_key_4.sql')
    
    def test_38_pg_constraint_alter_part_table_alter_type(self):
        '''Oid_inconsistency : pg_constaint_alter part_table alter data type -oid'''
        self.doTest('pg_constraint_alter_part_table_alter_type.sql')
        self.doTest('pg_constraint_alter_part_table_alter_type_2.sql')

    def test_39_pg_constraint_alter_table_add_default_part(self):
        '''Oid_inconsistency : pg_constaint_alter_table_add_default_part -oid'''
        self.doTest('pg_constraint_alter_table_add_default_part.sql')
   
    def test_40_pg_constraint_create_part_table_check(self):
        '''Oid_inconsistency : pg_constaint_create_part_table_check -oid'''
        self.doTest('pg_constraint_create_part_table_check.sql')
        self.doTest('pg_constraint_create_part_table_check_2.sql')
   
    def test_41_pg_constraint_alter_part_table_add_unique(self):
        '''Oid_inconsistency : pg_constaint alter_part_table_add_unique -oid'''
        self.doTest('pg_constraint_alter_part_table_add_unique.sql')
        self.doTest('pg_constraint_alter_part_table_add_unique_2.sql')

    def test_42_pg_constraint_alter_part_table_add_primary(self):
        '''Oid_inconsistency : pg_constaint alter_part_table_add_primary -oid'''
        self.doTest('pg_constraint_alter_part_table_add_primary.sql')
        self.doTest('pg_constraint_alter_part_table_add_primary_2.sql')
     
    def test_43_alter_table_with_oid(self):
        '''MPP-13870: Alter table Set Without Oids fails in case of inheritance'''
        sql_file = local_path('alter_table_with_oid.sql')
        out_file = local_path('alter_table_with_oid.out')
        ans_file = local_path('alter_table_with_oid.ans')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))
