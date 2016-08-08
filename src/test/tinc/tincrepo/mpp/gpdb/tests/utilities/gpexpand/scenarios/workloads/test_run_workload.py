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

import tinctest

from tinctest.lib.system import TINCSystem
from tinctest.lib import run_shell_command

from mpp.lib.config import GPDBConfig
from mpp.models import SQLTestCase

from gppylib.db import dbconn
from gppylib.db.dbconn import UnexpectedRowsError

@tinctest.skipLoading('scenario')
class PreExpansionWorkloadTests(SQLTestCase):

    sql_dir = 'pre_sql/'
    ans_dir = 'pre_expected/'
    out_dir = 'pre_output/'

    @classmethod
    def setUpClass(cls):
        super(PreExpansionWorkloadTests, cls).setUpClass()
        # gpscp the script required for external table in create_base_workload
        scp_file = os.path.join(cls.get_sql_dir(), 'datagen.py')
        gpconfig = GPDBConfig()
        hosts = gpconfig.get_hosts()
        hosts_file = os.path.join(cls.get_out_dir(), 'hostfile')
        with open(hosts_file, 'w') as f:
            f.write('\n'.join(hosts))
        
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command("gpscp -f %s %s =:$GPHOME/bin" %(hosts_file, scp_file), 'gpscp script', res)
        if res['rc'] > 0:
            tinctest.logger.warning("Failed to gpscp the required script to all the segments for external table queries. The script might already exist !")
        
    def get_substitutions(self):
        subst = {}
        config = GPDBConfig()
        host, _ = config.get_hostandport_of_segment(0)
        subst['@host@'] = 'rh55-qavm44'
        subst['@script@'] = os.path.join(self.get_sql_dir(), 'datagen.py')
        return subst
    
@tinctest.skipLoading('scenario')
class TableDistrubtionPolicySnapshot(SQLTestCase):
    """
    Generates a snapshot of distribution policies of all
    the tables in all the databases in the cluster. Depends on the previous
    test class that will generate all the test sqls

    @gpdiff False
    """

    sql_dir = 'distribution_policy_sql/'
    ans_dir = 'distribution_policy_expected/'
    out_dir = 'distribution_policy_snapshot_out/'
    generate_ans = 'force'

    @classmethod
    def setUpClass(cls):
        super(TableDistrubtionPolicySnapshot, cls).setUpClass()
        TINCSystem.make_dirs(cls.get_ans_dir(), force_create=True)

    def get_substitutions(self):
        substitutions = {}
        tinctest.logger.info("Generating distribution policy snapshot sql for all DBs ...")
        with dbconn.connect(dbconn.DbURL()) as conn:
            cmd = "select datname from pg_database where datname not in ('postgres', 'template1', 'template0')"
            rows = dbconn.execSQL(conn, cmd)
            sql_string = ''
            for row in rows:
                sql_string += '\\c %s \n' %row[0].strip()
                sql_string += """select n.nspname || '.' || c.relname as fq_name,  p.attrnums as distribution_policy
                                 FROM pg_class c JOIN pg_namespace n ON (c.relnamespace=n.oid) JOIN pg_catalog.gp_distribution_policy p on (c.oid = p.localoid)
                                 WHERE n.nspname != 'gpexpand' AND n.nspname != 'pg_bitmapindex' AND c.relstorage != 'x';"""
                sql_string += '\n'
        substitutions['@distribution_policy_snapshot_sql@'] = sql_string
        return substitutions

@tinctest.skipLoading('scenario')
class TableDistributionPolicyVerify(TableDistrubtionPolicySnapshot):
    """
    Verifies the distribution policies of all the tables in all the databases
    after expansion
    This uses the ans file that will be generated by the above test case.
    """
    sql_dir = 'distribution_policy_sql/'
    ans_dir = 'distribution_policy_expected/'
    out_dir = 'distribution_policy_verify_out/'
                    
@tinctest.skipLoading('scenario')
class SetRanks(SQLTestCase):
    """
    """
    sql_dir = 'ranks_sql/'
    ans_dir = 'ranks_expected/'
    out_dir = 'ranks_output/' 
                        

    
