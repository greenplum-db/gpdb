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
import time
import shutil
import unittest2 as unittest

from gppylib.db import dbconn
from gppylib.commands.base import Command
from gppylib.commands.gp import GpStart, GpStop

import tinctest
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase

class transactions(MPPTestCase):
    def test_skip_checkpoint_abort_transaction(self):
        """
        
        @description FATAL failure execution handles already committed transactions properly
        @created 2013-04-19 00:00:00
        @modified 2013-04-19 00:00:00
        @tags transaction checkpoint MPP-17817 MPP-17925 MPP-17926 MPP-17927 MPP-17928 schedule_transaction
        @product_version gpdb: [4.1.2.5- main]

        Repro steps:
        1. GPDB is up and running, number of segments is irrelevant, no master standby is required, 
        no segment mirroring is required
        2. inject fault on master for skipping checkpoints
        > gpfaultinjector -f checkpoint -m async -y skip -s 1 -o 0
        3. inject fault 'fatal' on master, it aborts already committed local transaction
        > gpfaultinjector -p 4100 -m async -s 1 -f local_tm_record_transaction_commit -y panic_suppress
        4. create table 'test'
        > psql template1 -c 'create table test(a int);'
        5. connect in utility mode to master and create table, insert rows into table and truncate table
        > PGOPTIONS='-c gp_session_role=utility -c allow_system_table_mods=dml' psql -p 4100 template1 
        begin; 
        create table test21(a int);
        insert into test21(a) values(10);
        truncate table test21;
        commit;
        6. Wait 5 minutes
        7. GPDB immediate shutdown and restart, GPDB does not come up with versions without fix, 
        GPDB comes up with versions with fix
        > gpstop -air
        """

        master_port = os.getenv("PGPORT", "5432")
        cmd = Command(name="gpfaultinjector", cmdStr="gpfaultinjector -f checkpoint -m async -y skip -s 1 -o 0")
        cmd.run()
        cmd = Command(name="gpfaultinjector", 
            cmdStr="gpfaultinjector -p %s -m async -s 1 \
                    -f local_tm_record_transaction_commit -y panic_suppress" % master_port)
        cmd.run()
        PSQL.run_sql_command("create table mpp17817(a int)")
        sql_file = local_path('mpp17817.sql')
        PSQL.run_sql_file(sql_file, PGOPTIONS="-c gp_session_role=utility")
        time.sleep(300)
        cmd = Command(name="gpstop restart immediate",
            cmdStr="source %s/greenplum_path.sh;\
                    gpstop -air" % os.environ["GPHOME"])
        cmd.run(validateAfter=True)

        # Cleanup
        PSQL.run_sql_command("drop table mpp17817")
        PSQL.run_sql_command("drop table mpp17817_21")
   
