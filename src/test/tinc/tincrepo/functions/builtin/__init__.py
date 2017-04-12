import os
import sys
import datetime
import math
import tinctest
import re
from time import gmtime, strftime

import unittest2 as unittest
from fnmatch import fnmatch

from tinctest.runner import TINCTextTestResult
from tinctest.lib import run_shell_command, Gpdiff

from mpp.lib.PSQL import PSQL
from mpp.models import SQLTestCase

class BuiltinFunctionTestCase(SQLTestCase):

    def __init__(self, methodName, baseline_result = None, sql_file = None, db_name = None):
        super(BuiltinFunctionTestCase, self).__init__(methodName, baseline_result, sql_file, db_name)
        self.optimizer_mode = 'on'

    def _infer_metadata(self):
        super(BuiltinFunctionTestCase, self)._infer_metadata()
        self.executemode= self._metadata.get('executemode', 'ORCA_PLANNER_DIFF')

    def run_test(self):
        sql_file = self.sql_file
        ans_file = self.ans_file
        
        source_file = sys.modules[self.__class__.__module__].__file__
        source_dir = os.path.dirname(source_file)
        out_directory = self.get_out_dir()

        if (self.executemode == 'ORCA_PLANNER_DIFF'):
            out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '.out'))
            new_ans_file = os.path.join(self.get_out_dir(), os.path.basename(ans_file).replace('.ans', '_mod.ans'))
            guc_sql_file = self._add_gucs_to_sql_file(sql_file)
            self.gucs.add('optimizer=off')
            self.gucs.add('optimizer_log=off')
            guc_off_sql_file = self._add_gucs_to_sql_file(sql_file)
    
            PSQL.run_sql_file(guc_off_sql_file, dbname = self.db_name, out_file = new_ans_file)
            PSQL.run_sql_file(guc_sql_file, dbname = self.db_name, out_file = out_file)

            pattern = 's/transactionid,,,,,,[[:digit:]]\+,,,,[[:digit:]]\+\/[[:digit:]]\+,[[:digit:]]\+,ExclusiveLock,t,[[:digit:]]\+,/transactionid,,,,,,TRANSACTIONID,,,,VIRTUAL\/XID,PID,ExclusiveLock,t,SESSIONID,/;s/virtualxid,,,,,[[:digit:]]\+\/[[:digit:]]\+,,,,,[[:digit:]]\+\/[[:digit:]]\+,[[:digit:]]\+,ExclusiveLock,t,[[:digit:]]\+,/virtualxid,,,,,VIRTUAL\/XID,,,,,VIRTUAL\/XID,PID,ExclusiveLock,t,SESSIONID,/'
            pattern += ';s@relation,\([[:digit:]]\+\),\([[:digit:]]\+\),,,,,,,,[[:digit:]]\+/[[:digit:]]\+,[[:digit:]]\+,AccessShareLock,t,[[:digit:]]\+,t,\([[:digit:]]\+\)@relation,\\1,\\2,,,,,,,,VIRTUAL/XID,PID,AccessShareLock,t,SESSIONID,t,\\3@'
            sedcmd = "sed -i '' -e '%(pattern)s' %(answer_file)s" % {"answer_file": new_ans_file, "pattern": pattern}
            sedcmd2 = "sed -i '' -e '%(pattern)s' %(answer_file)s" % {"answer_file" :out_file, "pattern": pattern}
            sedcmd3 = "sed -i '' -e 's/pg_aoseg_[[:digit:]]\+/pg_aoseg_XXXXX/' " +new_ans_file
            sedcmd4 = "sed -i '' -e 's/pg_aoseg_[[:digit:]]\+/pg_aoseg_XXXXX/' " +out_file
            run_shell_command(sedcmd, "replace dynamic values in planner output with XXXXX")
            run_shell_command(sedcmd2, "replace dynamic values in ORCA output with XXXXX")
            run_shell_command(sedcmd3, "replace dynamic values in pg_aoseg.pg_aoseg_")
            run_shell_command(sedcmd4, "replace dynamic values in pg_aoseg.pg_aoseg_")
    
            result = Gpdiff.are_files_equal(out_file, new_ans_file)
            if result == False:
                self.test_artifacts.append(out_file.replace('.out', '.diff'))
    
            return result
        elif (self.executemode == 'gt1'):
            out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '.out'))
            guc_sql_file = self._add_gucs_to_sql_file(sql_file)
            
            PSQL.run_sql_file(guc_sql_file, dbname = self.db_name, out_file = out_file)
            f = open(out_file, 'r')
            content = f.readlines()
            f.close()
            linecount = 0
            for x in content:
                x = x.strip()
                if x == "(1 row)":
                    output = content[linecount - 1].strip()
                linecount += 1
                    
            if int(output) > 1:
                return True
            diff_file = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '.diff'))
            f = open(diff_file, 'w')
            f.write("expecting an output which is greater than 1, instead the actual output was " +output)
            f.close()
            return False
        else:
            out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '.out'))
            guc_sql_file = self._add_gucs_to_sql_file(sql_file)
            
            PSQL.run_sql_file(guc_sql_file, dbname = self.db_name, out_file = out_file)

            sedcmd1 = "sed -i -e 's/transactionid,,,,,[[:digit:]]\+,,,,[[:digit:]]\+,[[:digit:]]\+,ExclusiveLock,t,[[:digit:]]\+,/transactionid,,,,,XXXXX,,,,XXXXX,XXXXX,ExclusiveLock,t,XXXXX,/' " +out_file
            sedcmd2 = "sed -i -e 's/pg_aoseg_[[:digit:]]\+/pg_aoseg_XXXXX/' " +out_file
            run_shell_command(sedcmd1, "replace dynamic values in ORCA output with XXXXX")
            run_shell_command(sedcmd2, "replace dynamic values in pg_aoseg.pg_aoseg_")
            
            result = Gpdiff.are_files_equal(out_file, ans_file)
            if result == False:
                self.test_artifacts.append(out_file.replace('.out', '.diff'))
            return result
            
