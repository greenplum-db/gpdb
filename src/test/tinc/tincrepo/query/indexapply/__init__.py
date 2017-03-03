import os
import sys
import datetime
import math
import tinctest
import re
import shutil
from time import gmtime, strftime, sleep

import unittest2 as unittest
from fnmatch import fnmatch
from tinctest.runner import TINCTextTestResult
from tinctest.lib import run_shell_command, Gpdiff

from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL

class IndexApplyTestCase(SQLTestCase):
    ignore_fallback_with_indexapply_off = False

    ## generate_ans = 'force'

    def __init__(self, methodName, baseline_result = None, sql_file = None, db_name = None):
        self.negativetest = 'None'
        self.datadir = self.get_out_dir()
        super(IndexApplyTestCase, self).__init__(methodName, baseline_result, sql_file, db_name)

    def _infer_metadata(self):
        super(IndexApplyTestCase, self)._infer_metadata()
        self.negativetest = self._metadata.get('negativetest', 'False')
        self.bitmapindex = self._metadata.get('bitmapindex', 'off')

    def run_test(self):
        failreason = ''
        sql_file = self.sql_file
        ans_file = self.ans_file

        source_file = sys.modules[self.__class__.__module__].__file__
        source_dir = os.path.dirname(source_file)
        out_directory = os.path.join(source_dir, self.get_out_dir())

        ##get the query from sqlfile - multi-line okay
        syscmd = "sed -e 's/^\-\-.*\@.*//' " +sql_file
        p = os.popen(syscmd, 'r')
        unstripped_query = p.readlines()
        query = []
        for x in unstripped_query:
            query.append(x.strip())

        #create explain sql with orca on and force index-apply, check we're really using index apply
        sql_file_explain_indexapply_on = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_explain_indexapply_on.sql'))
        out_file_explain_indexapply_on = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_explain_indexapply_on.out'))
        syscmd = "rm -f " +sql_file_explain_indexapply_on
        os.popen(syscmd, 'r')

        plan_stmt = ""
        try_all_plans = self._metadata.get('try_all_plans', 'False')
        if try_all_plans == 'True':
            success, plan_stmt_or_error_message = self.find_plan_with_index(query, sql_file_explain_indexapply_on, out_file_explain_indexapply_on)
            if success:
                plan_stmt = plan_stmt_or_error_message
            else:
                failreason += plan_stmt_or_error_message
        else:
            success, error_message = self.run_explain_indexjoin_on(query, sql_file_explain_indexapply_on, out_file_explain_indexapply_on, "", 0)
            if not success:
                failreason += error_message

        #run explain sql file with indexapply off, make sure index-scan is not being used, use this as ans file.
        sql_file_explain_indexapply_off = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_explain_indexapply_off.sql'))
        out_file_explain_indexapply_off = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_explain_indexapply_off.out'))
        syscmd = "rm -f " +sql_file_explain_indexapply_off
        os.popen(syscmd, 'r')

        sqlcontent = [
            "--start_ignore",
            "set optimizer=on;",
            "set optimizer_enable_indexjoin=off;",
            "set optimizer_enable_bitmapscan=off;",
            "select enable_xform('CXformInnerJoin2NLJoin');",
            "select enable_xform('CXformInnerJoin2HashJoin');",
            "set client_min_messages='log';",
            "--end_ignore",
            "EXPLAIN" ]
        for x in query:
            sqlcontent.append(x)
        self._add_tofile(sql_file_explain_indexapply_off, sqlcontent, 0)

        PSQL.run_sql_file(sql_file_explain_indexapply_off, dbname = self.db_name, out_file = out_file_explain_indexapply_off)

        if (self._look_for_string(out_file_explain_indexapply_off, "Planner produced plan :1") == True):
            if self.ignore_fallback_with_indexapply_off:
                sys.stderr.write('Unexpected fallback with force indexapply off\n')
            else:
                failreason += "Unexpected fallback with force indexapply off |"
        elif (self._look_for_string(out_file_explain_indexapply_off, "Index Cond", check_join_cond = True) == True):
            failreason += "Index Scan used when optimizer_enable_indexjoin is off "


        #add force indexApply to sql_file
        sql_file_run = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_run.sql'))
        out_file_run = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_run.out'))
        syscmd = "rm -f " +sql_file_run
        os.popen(syscmd, 'r')
        sqlcontent = [
            "--start_ignore",
            "set optimizer=on;",
            "set optimizer_enable_indexjoin=on;",
            "set optimizer_enable_bitmapscan=on;",
            "select enable_xform('CXformInnerJoin2NLJoin');",
            "select enable_xform('CXformInnerJoin2HashJoin');" if not plan_stmt else "select disable_xform('CXformInnerJoin2HashJoin');",
            plan_stmt,
            "--end_ignore"]
        for x in query:
            sqlcontent.append(x)
        self._add_tofile(sql_file_run, sqlcontent, 0)
        self.sql_file = sql_file_run

        PSQL.run_sql_file(sql_file_run, dbname = self.db_name, out_file=out_file_run)
        if self.generate_ans != 'yes' and self.generate_ans != 'force':
            if not Gpdiff.are_files_equal(out_file_run, self.ans_file):
                failreason = "There's a diff between:" +out_file_run+ " and " +self.ans_file
        else:
            shutil.copyfile(out_file_run, self.ans_file)

        if failreason != '':
            failedfile = os.path.join(out_directory, 'failed.out')
            f = open(failedfile , 'a')
            f.write(os.path.basename(sql_file)+ " | " +failreason+ "\n")
            f.close()
            self.fail(failreason)
        else:
            return True


    def _add_tofile(self, filename, lines, pos):
        """
        @param filename we're adding line to
        @param line to add to the file
        """
        contents = []
        if os.path.isfile(filename):
            f = open(filename, 'r')
            contents = f.readlines()
            f.close()
        y = open(filename, 'w')
        count = len(lines) - 1
        while int(count) >= 0:
            contents.insert(int(pos), lines[count]+ '\n')
            count -= 1

        contents = "".join(contents)
        y.write(contents)
        y.close()

    def _look_for_string(self, filename, str, check_join_cond = False):
        """
        search for a specific string in the given file
        """
        f = open(filename, 'r')
        contents = f.readlines()
        f.close()

        if check_join_cond == True:
            for line in contents:
                if str in line:
                    if "x." in line and ("y." in line or "\"outer\"." in line):
                        return True
                    if "y." in line and ("x." in line or "\"outer\"." in line):
                        return True
            return False
        else:
            for line in contents:
                if str in line:
                    return True
            return False

    def find_plan_with_index(self, query, explain_file, explain_file_out):
        """
        iterate over all plans and find the first one that has an index scan
        if such a plan is found, return True and the gucs to pick that plan
        otherwise return False and an error message
        """
        sqlcontent = [
            "-- start_ignore",
            "set optimizer=on;",
            "set optimizer_enable_indexjoin=on;",
            "set optimizer_enable_bitmapscan=on;",
            "select disable_xform('CXformInnerJoin2HashJoin');",
            "set client_min_messages=\'log\';",
            "set optimizer_enumerate_plans=on;",
            "-- end_ignore",
            "EXPLAIN" ]
        for x in query:
            sqlcontent.append(x)
        self._add_tofile(explain_file, sqlcontent, 0)

        # Get the number of alternative plans from explain plan
        self._add_tofile(explain_file, sqlcontent, 0)

        PSQL.run_sql_file(explain_file, dbname = self.db_name, out_file = explain_file_out)

        syscmd = "grep \'Number of plan alternatives:\' " + explain_file_out + " |sed -e \'s/^.*\[OPT\]:.*alternatives: //\'"
        q = os.popen(syscmd, "r")
        alternatives_line = q.readline()
        q.close()
        try:
            actual_plan_id = int(alternatives_line.strip())
        except ValueError:
            print " No plan generated "
            result_str = 'No plan generated for the query in %s.\n Please check %s for the explain plan.' %(self.sql_file,explain_file)
            return (False, result_str)

        if actual_plan_id == 0:
            # Query Crashes.
            return (False, "Number of plan alternatives with index join on: 0")

        for planid in range(1, actual_plan_id + 1):
            # Run the SQL file for the different plan ids
            planid_stmt = "set optimizer_enumerate_plans=on; set optimizer_plan_id=%d;" % planid
            success, f = self.run_explain_indexjoin_on(query, explain_file, explain_file_out, planid_stmt, planid)            
            if success:
                return (True, planid_stmt)
        return False, "No plan for query %s has index scan" % self.sql_file


    def run_explain_indexjoin_on(self, query, explain_file, explain_file_out, planid_stmt, planid):
        syscmd = "rm -f " + explain_file
        os.popen(syscmd, 'r')
        sqlcontent = [
            "--start_ignore",
            "set optimizer=on;",
            "set optimizer_enable_indexjoin=on;",
            "set optimizer_enable_bitmapscan=on;",
            "select disable_xform('CXformInnerJoin2NLJoin');" if not planid_stmt else "",
            "select disable_xform('CXformInnerJoin2HashJoin');",
            planid_stmt,
            "set client_min_messages='log';",
            "--end_ignore",
            "EXPLAIN" ]
        for x in query:
            sqlcontent.append(x)
        self._add_tofile(explain_file, sqlcontent, 0)
            
        PSQL.run_sql_file(explain_file, dbname = self.db_name, out_file = explain_file_out)

        if planid != 0:
            new_file = explain_file_out+str(planid)
            syscmd_cp = "cp %s %s" % (explain_file_out, new_file)
            os.popen(syscmd_cp, 'r')

        if (self._look_for_string(explain_file_out, "Planner produced plan :1") == True):
            return False, "Unexpected fallback with force indexapply on |"
        elif (self._look_for_string(explain_file_out, "Index Cond", check_join_cond = True) == False) and self.negativetest == 'False':
            return False, "Index Scan in join not being used with force indexapply | " + explain_file_out
        elif (self._look_for_string(explain_file_out, "Index Cond", check_join_cond = True) == True) and self.negativetest == 'True':
            return False, "Index Scan in join is being used when it shouldn't | "
        else:
            return True, None
