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
import re
import unittest2 as unittest

import tinctest

from tinctest.runner import TINCTextTestResult
from tinctest.lib import Gpdiff

from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL

@tinctest.skipLoading("Test model. No tests loaded.")
class SQLPerformanceTestCase(SQLTestCase):
    """
    @metadata: repetitions: number of times the sql should be executed (default: 3)
    @metadata: threshold: if the current performance of the query is worse than baseline by this much 
                          percentage, the test is marked failed (default: 5)
    @metadata: timeout: number of seconds to wait for the query to complete. When timeout is reached,
                        the query is terminated. Value of 0 means no timeout. (default: 0)
    @metadata: drop_caches: whether to drop system cache and restart cluster before running each query (default: True)
    @metadata: explain: whether to gather explain plan details for each query (default: True)
    @metadata: plandiff: tdb
    """

    def __init__(self, methodName, baseline_result = None, sql_file=None, db_name = None):
        self.repetitions = None
        self.threshold = None
        self.timeout = None
        self.plandiff = True
        self.doexplain = True
        self.drop_caches = True
        self._runtime = -1.0
        self._result_string = None
        # Add field to store plan body
        self._plan_body = ''
        # Switch to control whether to avoid run test or not
        self._avoid_execution = False;
        # Add dict to store optimization times
        self._optimization_time = {};
        # Add string to store explain analyze output
        self._ea_output = ''
        super(SQLPerformanceTestCase, self).__init__(methodName, baseline_result, sql_file, db_name)
        self.gucs.add('statement_timeout='+str(self.timeout))

    def _infer_metadata(self):
        super(SQLPerformanceTestCase, self)._infer_metadata()
        self.repetitions = int(self._metadata.get('repetitions', '3'))
        self.threshold = int(self._metadata.get('threshold', '5'))
        self.timeout = int(self._metadata.get('timeout', '0'))  # 0 means unbounded by default.
        if self._metadata.get('drop_caches', 'True') == 'False':
            self.drop_caches = False
        if self._metadata.get('explain', 'True') == 'False':
            self.doexplain = False
        if self._metadata.get('plandiff', 'True') == 'False':
            self.plandiff = False

    def setUp(self):
        # Setup the database by calling out to the super class
        tinctest.logger.trace_in()
        super(SQLPerformanceTestCase, self).setUp()

        #Collect explain output and then compare with that of the last run
        if self.doexplain:
            self._compare_previous_plan()
        tinctest.logger.trace_out()
    
    def _compare_previous_plan(self):
        """
        Get plan first and then compare with that of the previous run. If nothing change in the plan structure,
        there is no need to re-execute that query. The result will be copied from the previous run.
        """
        #execute the explain sql to fetch plan
        explain_sql_file = os.path.join(self.get_out_dir(), os.path.basename(self.sql_file).replace('.sql','_explain.sql'))
        with open(explain_sql_file, 'w') as o:
            with open(self.sql_file, 'r') as f:
                explain_write = False
                for line in f:
                    if not line.startswith('--') and not explain_write:
                        #keep all the GUCs
                        o.write('-- start_ignore\n')
                        for guc_string in self.gucs:
                            o.write("SET %s;" %guc_string)
                            o.write(line)
                        for orca_guc_string in self.orcagucs:
                            o.write("%s;\n"%orca_guc_string)
                        # Add gucs to print optimization time to log
                        o.write("SET optimizer_print_optimization_stats=on;\n")
                        o.write("SET client_min_messages='log';\n")
                        o.write("SELECT gp_opt_version();\n")
                        o.write("SELECT current_timestamp;\n")
                        o.write('-- end_ignore\n')
                        o.write('explain %s' %line)
                        explain_write = True
                    else:
                        o.write(line);
        explain_out_file = os.path.join(self.get_out_dir(), os.path.basename(explain_sql_file).replace('.sql','.out'))
        tinctest.logger.info("Gathering explain from sql : " + explain_sql_file)
        PSQL.run_sql_file(explain_sql_file, dbname = self.db_name, out_file = explain_out_file)
        # rewrite plan to keep plan body
        self._rewrite_plan_file(explain_out_file)

        # retrieve previous plan and store it into local file
        if self.baseline_result and self.plandiff:
            if self.baseline_result.result_detail:
                if 'plan_body' in self.baseline_result.result_detail.keys():
                    previous_explain_output = self.baseline_result.result_detail['plan_body']
                    previous_explain_output_file = explain_out_file.replace('.out','_previous.out')
                    with open(previous_explain_output_file, 'w') as o:
                        o.write(previous_explain_output)
                    # call GPDiff to compare two plans
                    if Gpdiff.are_files_equal(previous_explain_output_file, explain_out_file):
                        # two plans are the same, avoid execution
                        self._avoid_execution = True
                        self._runtime = self.baseline_result.value  # copy the runtime from previous result
                        self._result_string = self.baseline_result.result_string
                        # comment it out as we are experiencing some problems during parse.
                        if 'explain_analyze' in self.baseline_result.result_detail.keys():
                            tmp_ea = self.baseline_result.result_detail['explain_analyze']
                            self._ea_output = tmp_ea.replace('\\','')
                            if len(self._ea_output) == 0: # if there is no previous explain analyze output, generate it
                                self._generate_explain_analyze_output()
                    else:
                        self._generate_explain_analyze_output()

    def _generate_explain_analyze_output(self):
        """
        execute explain analyze output for a given query
        """
        ea_sql_file = os.path.join(self.get_out_dir(), os.path.basename(self.sql_file).replace('.sql','_explain_analyze.sql'))
        with open(ea_sql_file, 'w') as o:
            with open(self.sql_file, 'r') as f:
                explain_write = False
                for line in f:
                    if not line.startswith('--') and not explain_write:
                        #keep all the GUCs
                        o.write('-- start_ignore\n')
                        for guc_string in self.gucs:
                            o.write("SET %s;" %guc_string)
                            o.write(line)
                        for orca_guc_string in self.orcagucs:
                            o.write("%s;\n"%orca_guc_string)
                        # Add gucs to print optimization time to log
                        o.write("SET optimizer_print_optimization_stats=on;\n")
                        o.write("SET client_min_messages='log';\n")
                        o.write('-- end_ignore\n')
                        o.write('explain analyze %s' %line)
                        explain_write = True
                    else:
                        o.write(line);
        ea_out_file = ea_sql_file.replace('.sql','.out')
        PSQL.run_sql_file(ea_sql_file, dbname = self.db_name, out_file = ea_out_file)
        with open(ea_out_file, 'r') as f:
            self._ea_output = f.read()


    def _rewrite_plan_file(self, explain_out_file):
        """
        rewrite explain output to keep only GUC info and plan body
        """

        # initialize time variable
        dxl_query_serialization_time = 0.0
        dxl_expr_translation_time = 0.0
        group_merge_time = 0.0
        total_opt_time = 0.0
        stats_derivation_time = 0.0
        expr_dxl_translation_time = 0.0
        dxl_plan_serialization_time = 0.0

        guc_plan_content = ''
        with open(explain_out_file, 'r') as f:
            able_to_write = True
            fall_back_check = False
            for line in f:
                # ignore the part that from '-- end_ignore' to 'QUERY PLAN'
                if line.startswith('-- end_ignore'):
                    guc_plan_content += line
                    able_to_write = False
                elif line.find('QUERY PLAN') != -1:
                    guc_plan_content += '-- force_explain\n'
                    able_to_write = True
                if able_to_write:
                    guc_plan_content += line

                # collect total optimization time and statistics derivation time
                if line.find("Statistics Derivation Time")!=-1:
                    try:
                        stats_derivation_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        stats_derivation_time = -1.0
                elif line.find("Total Optimization Time")!=-1:
                    try:
                        total_opt_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        total_opt_time = -1.0
                elif line.find("DXL Query Serialization Time")!=-1:
                    try:
                        dxl_query_serialization_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        dxl_query_serialization_time = -1.0
                elif line.find("DXL To Expr Translation Time")!=-1:
                    try:
                        dxl_expr_translation_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        dxl_expr_translation_time = -1.0
                elif line.find("Group Merge Time")!=-1:
                    try:
                        group_merge_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        group_merge_time = -1.0
                elif line.find("Expr To DXL Translation Time")!=-1:
                    try:
                        expr_dxl_translation_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        expr_dxl_translation_time = -1.0
                elif line.find("DXL Plan Serialization Time")!=-1:
                    try:
                        dxl_plan_serialization_time = float(line[line.rindex(':')+1:line.rindex('ms')].strip())
                    except ValueError:
                        dxl_plan_serialization_time = -1.0
                elif line.find('Planner produced plan :0')!=-1 and fall_back_check == False:
                    fall_back_stats_path = os.path.join(self.get_out_dir(), 'fall_back_stats.txt')
                    existing = os.path.exists(fall_back_stats_path)
                    mode = 'a' if existing else 'w'
                    with open(fall_back_stats_path, mode) as f:
                        f.write('%s Expected fall back\n'%self.sql_file)
                    fall_back_check = True
                elif line.find('Planner produced plan :1')!=-1 and fall_back_check == False:
                    fall_back_stats_path = os.path.join(self.get_out_dir(), 'fall_back_stats.txt')
                    existing = os.path.exists(fall_back_stats_path)
                    mode = 'a' if existing else 'w'
                    with open(fall_back_stats_path, mode) as f:
                        f.write('%s Unexpected fall back\n'%self.sql_file)
                    fall_back_check = True

        self._optimization_time['total_opt_time'] = total_opt_time
        self._optimization_time['statistics_time'] = stats_derivation_time
        self._optimization_time['dxl_query_serialization_time'] = dxl_query_serialization_time
        self._optimization_time['dxl_expr_translation_time'] = dxl_expr_translation_time
        self._optimization_time['group_merge_time'] = group_merge_time
        self._optimization_time['expr_dxl_translation_time'] = expr_dxl_translation_time
        self._optimization_time['dxl_plan_serialization_time'] = dxl_plan_serialization_time

        self._plan_body = guc_plan_content
        with open(explain_out_file, 'w') as o:
            o.write(guc_plan_content)

    def run_test(self):
        """
        The method that subclasses should override to execute a sql test case differently.
        This encapsulates the execution mechanism of SQLTestCase. Given a base sql file and
        an ans file, runs all the sql files for the test case.

        Note that this also runs the other part sqls that make up the test case. For eg: if the
        base sql is query1.sql, the part sqls are of the form query1_part*.sql in the same location
        as the base sql.
        """
        tinctest.logger.trace_in()
        sql_file = self.sql_file
        ans_file = self.ans_file
        # if the plan is the same as previous one, skip this run
        if self._avoid_execution:
            tinctest.logger.info("Skipping test execution as there is no plan change w.r.t previous run.")
            str_runtime_list = []
            for i in range(self.repetitions):
                str_runtime_list.append(str(self._runtime))
            # dump statistics to a runtime_stats.csv file
            output_file_path = os.path.join(self.get_out_dir(), 'runtime_stats.csv')
            existing = os.path.exists(output_file_path)
            mode = 'a' if existing else 'w'
            with open(output_file_path, mode) as f:
                f.write("%s,%s\n" % (os.path.basename(sql_file), ",".join(str_runtime_list)))
            if self._result_string == 'FAIL' or self._result_string == 'ERROR':
                tinctest.logger.trace_out("False")
                return False
            else:
                tinctest.logger.trace_out("True")
                return True

        guc_sql_file = self._add_gucs_to_sql_file(sql_file)
        runtime_list = []
        for i in range(self.repetitions):
            # refresh the caches after each iteration
            if self.drop_caches:
                self._restart_cluster(refresh_cache=True)

            runtime_list.append(self._run_and_measure_sql_file(guc_sql_file, i, ans_file))

        # dump statistics to a runtime_stats.csv file
        str_runtime_list = [str(x) for x in runtime_list]
        output_file_path = os.path.join(self.get_out_dir(), 'runtime_stats.csv')
        existing = os.path.exists(output_file_path)
        mode = 'a' if existing else 'w'
        with open(output_file_path, mode) as f:
            f.write("%s,%s\n" % (os.path.basename(sql_file), ",".join(str_runtime_list)))

        self._runtime = min(runtime_list)
        tinctest.logger.trace_out("True")
        return True

    def _run_and_measure_sql_file(self, sql_file, iteration, ans_file = None):
        """
        Given a sql file and an ans file, this adds the specified gucs (self.gucs) to the sql file , runs the sql
        against the test case databse (self.db_name) and verifies the output with the ans file.
        """
        result = True

        self.test_artifacts.append(sql_file)
        out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace(".sql","_iter_%s.out" %iteration))
        self.test_artifacts.append(out_file)

        PSQL.run_sql_file(sql_file, dbname = self.db_name, out_file = out_file)

        if ans_file is not None:
            self.test_artifacts.append(ans_file)
            result = Gpdiff.are_files_equal(out_file, ans_file)
            if result == False:
                self.test_artifacts.append(out_file.replace('.out', '.diff'))
                self.fail('Diff failed between %s and %s' %(out_file, ans_file))

        return self._get_runtime(out_file)

    def _get_runtime(self, out_file):
        """
        Matches pattern <Time: 123.25 ms> and returns the sum of all the values found in the out file
        """
        total_time = 0.0
        with open(out_file, 'r') as f:
            for line in f:
                if re.match('^Time: \d+\.\d+ ms', line):
                    total_time += float(line.split()[1])
        return total_time

    def _add_gucs_to_sql_file(self, sql_file, gucs_sql_file=None, optimizer=None):
        """
        Form test sql file by adding the defined gucs to the sql file
        @param sql_file Path to  the test sql file
        @param gucs_sql_file Path where the guc sql file should be generated.
        @param optimizer Boolean that specifies whether optimizer is on or off.
        """
        if not gucs_sql_file:
            gucs_sql_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file))
            
        with open(gucs_sql_file, 'w') as o:
            gucs_write = False
            with open(sql_file, 'r') as f:
                for line in f:
                    if (line.find('--') != 0) and not gucs_write:
                        # Add gucs and then add the line
                        o.write('\n-- start_ignore\n')
                        for guc_string in self.gucs:
                            o.write("SET %s;\n" %guc_string)
                        for orca_guc_string in self.orcagucs:
                            o.write("%s;\n"%orca_guc_string)
                        gucs_write = True

                        # Write optimizer mode
                        optimizer_mode_str = ''
                        if optimizer is not None:
                            optimizer_mode_str = 'on' if optimizer else 'off'
                        if optimizer_mode_str:
                            o.write("SET optimizer=%s;\n" %optimizer_mode_str)

                        if optimizer is not None and optimizer:
                            for guc_string in self._optimizer_gucs:
                                o.write("SET %s;\n" %guc_string)

                        o.write('\\timing on\n')
                        o.write('\n-- end_ignore\n')
                        o.write(line)
                    else:
                        o.write(line)
        self.test_artifacts.append(gucs_sql_file)
        return gucs_sql_file

    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        if stream and descriptions and verbosity:
            return SQLPerformanceTestCaseResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()

class SQLPerformanceTestCaseResult(TINCTextTestResult):

    def __init__(self, stream, descriptions, verbosity):
        super(SQLPerformanceTestCaseResult, self).__init__(stream, descriptions, verbosity)

    def addSuccess(self, test):
        # Add test._runtime to result.value
        self.value = test._runtime
        super(SQLPerformanceTestCaseResult, self).addSuccess(test)

    def addFailure(self, test, err):
        """
        Collect explain plan and an explain analyze output
        """
        self.value = test._runtime
        super(SQLPerformanceTestCaseResult, self).addFailure(test, err)
