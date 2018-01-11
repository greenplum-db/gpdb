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

from gppylib.commands.base import Command

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from tinctest.lib import Gpdiff
import unittest2 as unittest

import os
import subprocess
import sys

base_dir = os.path.dirname(os.path.realpath(__file__))
gpdiff_init_file = os.path.join(base_dir, "sql", "init_file")

class UAO_FaultInjection_TestCase(MPPTestCase):
    
    def get_sql_files(self, name):
        sql_file = os.path.join(
            base_dir, "sql", name + ".sql");    
        out_file = os.path.join(base_dir, 
            "output", os.path.basename(sql_file).replace('.sql','.out'))
        ans_file = os.path.join(base_dir, 
            "expected", os.path.basename(sql_file).replace('.sql','.ans'))
        return (sql_file, out_file, ans_file)

    def test_uao_crash_compaction_before_cleanup_phase(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_setup")[0]
        (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))

        PSQL.run_sql_file(setup_file)
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_cleanup_phase_master(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_setup")[0]
        (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_master_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_master_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_master_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_master_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_cleanup_phase_master_with_aocs(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_with_aocs1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_with_aocs2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_master_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_master_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_master_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_master_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_cleanup_phase_master_with_ao(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_with_aocs_setup")[0]
        (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_with_ao1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_master_with_ao2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_master_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_master_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_master_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_master_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_cleanup_phase_segment_with_ao(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_seg_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_seg_with_ao1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_seg_with_ao2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_cleanup_phase_segment_with_aocs(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_seg_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_seg_with_aocs1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_cleanup_phase_seg_with_aocs2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_cleanup_phase -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_drop(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_drop_setup")[0]
        (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_crash_compaction_before_drop1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_drop2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))

        PSQL.run_sql_file(setup_file)
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)        
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_drop_master(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_drop_master_setup")[0]
        (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_crash_compaction_before_drop_master1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_drop_master2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_master_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_master_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_master_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_master_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_drop_master_with_aocs(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_drop_master_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_drop_master_with_aocs1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_drop_master_with_aocs2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_master_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_master_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_master_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_master_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_drop_master_with_ao(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_drop_master_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_drop_master_with_ao1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_drop_master_with_ao2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_master_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_master_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_master_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_master_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_drop_segment_with_ao(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_drop_seg_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_drop_seg_with_ao1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_drop_seg_with_ao2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_before_drop_segment_with_aocs(self):
        setup_file = self.get_sql_files("uao_crash_compaction_before_drop_seg_with_aocs_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_compaction_before_drop_seg_with_aocs1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_before_drop_seg_with_aocs2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 1'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_compaction_vacuum(self):
        setup_file = self.get_sql_files("uao_crash_compaction_vacuum_setup")[0]
        (sql_file1, out_file1, ans_file1) = self.get_sql_files("uao_crash_compaction_vacuum1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_compaction_vacuum2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f compaction_before_segmentfile_drop -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_update_in_tran(self):
        setup_file = self.get_sql_files("uao_crash_update_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_update_intran1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_update_intran2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))

        # We set the fault in appendonly_update and appendonly_insert
        # because planner will go through appendonly_update and ORCA
        # will do appendonly_delete and appendonly_insert
        set_fault_in_seg_panic_update = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_update -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset_update = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_update -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_panic_insert = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset_insert = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic_update)
        gpfaultinjector.run()
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic_insert)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset_update)
        gpfaultinjector.run()
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset_insert)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uaocs_crash_update_in_tran(self):
        setup_file = self.get_sql_files("uaocs_crash_update_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uaocs_crash_update_intran1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uaocs_crash_update_intran2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))

        # We set the fault in appendonly_update and appendonly_insert
        # because planner will go through appendonly_update and ORCA
        # will do appendonly_delete and appendonly_insert
        set_fault_in_seg_panic_update = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_update -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset_update = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_update -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_panic_insert = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset_insert = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic_update)
        gpfaultinjector.run()
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic_insert)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset_update)
        gpfaultinjector.run()
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset_insert)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uaocs_crash_update_with_ins_fault(self):
        setup_file = self.get_sql_files("uaocs_crash_update_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uaocs_crash_update_with_ins_fault1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uaocs_crash_update2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_update_with_ins_fault(self):
        setup_file = self.get_sql_files("uaocs_crash_update_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_update_with_ins_fault1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_update2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)

    def test_uao_crash_vacuum_with_ins_fault(self):
        setup_file = self.get_sql_files("uaocs_crash_update_setup")[0]
        (sql_file1, out_file1,ans_file1) = self.get_sql_files("uao_crash_vacuum1")
        (sql_file2, out_file2, ans_file2) = self.get_sql_files("uao_crash_vacuum2")
        if not os.path.exists(os.path.dirname(out_file1)):
            os.mkdir(os.path.dirname(out_file1))
        set_fault_in_seg_panic = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y panic --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        set_fault_in_seg_reset = 'source %s/greenplum_path.sh;gpfaultinjector -p %s -f appendonly_insert -t foo -y reset --seg_dbid 2'  % (os.getenv('GPHOME'), os.getenv('PGPORT'))
        cmd_type = 'fault injector'

        PSQL.run_sql_file(setup_file)
        gpfaultinjector = Command(cmd_type, set_fault_in_seg_panic)

        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file1, out_file=out_file1)
    
        result1 = Gpdiff.are_files_equal(out_file1, ans_file1, match_sub=[gpdiff_init_file])

        PSQL.wait_for_database_up();

        gpfaultinjector = Command(cmd_type, set_fault_in_seg_reset)
        gpfaultinjector.run()

        PSQL.run_sql_file(sql_file2, out_file=out_file2)
        result2 = Gpdiff.are_files_equal(out_file2, ans_file2, match_sub=[gpdiff_init_file])

        self.assertTrue(result1)
        self.assertTrue(result2)
