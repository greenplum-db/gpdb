import os
import sys
import fileinput
import tinctest

import unittest2 as unittest
from fnmatch import fnmatch
from mpp.models import SQLTestCase
from tinctest.runner import TINCTextTestResult
from mpp.lib.PSQL import PSQL
from tinctest.lib import run_shell_command, Gpdiff

class PartitionIndexesSQLTestCase(SQLTestCase):

    def _infer_metadata(self):
        super(PartitionIndexesSQLTestCase, self)._infer_metadata()
        self.negtest = self._metadata.get('negtest', 'False')
        self.optimizer_mode = 'on'

    def run_test(self):
        sql_file = self.sql_file
        ans_file = self.ans_file
        
        nonnegflag = 0
        source_file = sys.modules[self.__class__.__module__].__file__
        out_directory = self.get_out_dir()
        out_file = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '.out'))
        guc_sql_file = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_gucs.sql'))

        # Create the sql file with GUCS
        cmd = "cp " + sql_file + " " + guc_sql_file
        shellcmd = "Creating %s" %os.path.basename(guc_sql_file)
        run_shell_command(cmd, shellcmd)

        # Add the GUCS to the new sql file
        self._add_linetofile(guc_sql_file,'SET optimizer=on;SET optimizer_log=on;select disable_xform(\'CXformDynamicGet2DynamicTableScan\');\n')

        # Deal with the positive test cases
        if self.negtest == 'False':
            nonnegflag = 1
            #Create the file containing the plan
            explain_file_sql = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_explain.sql'))
            explain_file_out = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_explain.out'))

            cmd = "cp " + guc_sql_file + " " + explain_file_sql
            shellcmd = "Creating %s" %explain_file_sql
            run_shell_command(cmd, shellcmd)

            # Remove metadata from explain SQL file
            cmd = "sed -i '/^\-\- \@.*/d' " + explain_file_sql
            run_shell_command(cmd, "Remove the metadata info for easier post-processing")

            sedcmd= "sed -i -e 's/^SELECT \(.*\) ORDER\(.*\)\;/EXPLAIN SELECT \\1\;/' %s" %explain_file_sql
            run_shell_command(sedcmd, "Adding explain SQL")

            # Get explain output
            PSQL.run_sql_file(explain_file_sql, dbname = self.db_name, out_file = explain_file_out)
            # Validation part#1: check if plan has dynamic index scan
            syscmd = "grep \"Dynamic Index Scan\" " + explain_file_out + " | wc -l "
            p = os.popen(syscmd, 'r')
            numindexscans = p.readline().strip()

        # Run the actual SQL file
        PSQL.run_sql_file(guc_sql_file, dbname = self.db_name, out_file = out_file)

        result = True
        if nonnegflag == 1 and int(numindexscans) <= 0:
            result_str  = "FAIL: Dynamic Index Scan not used"
            self.resultobj = result_str
            result = False
        else:
            self.resultobj = "No need to check query plan. Test case validated with gpdiff"
            # Validation part#2: check for wrong results
            return Gpdiff.are_files_equal(out_file, ans_file )

        if result == False:
            extra_diff_file = os.path.join(out_directory, os.path.basename(sql_file).replace('.sql', '_extra_failure.diff'))
            cmd = "echo \"" + result_str+ "\" > " + extra_diff_file
            run_shell_command(cmd, "Adding failure to extra diff file")
            cmd = "echo \"Check %s for the explain plan with optimizer ON.\" >> %s" %(explain_file_out,extra_diff_file)
            run_shell_command(cmd, "Adding failure to extra diff file")
        return result

    def _add_linetofile(self,guc_sql_file, content):
        """
        @param guc_sql_file we're adding line to
        @param content gucs to add to the file
        """
        newcontent = '-- start_ignore\n' + content + '-- end_ignore\n'
        for linenum,line in enumerate( fileinput.FileInput(guc_sql_file,inplace=1) ):
            if linenum==0:
                print newcontent
                print line.rstrip()
            else:
                print line.rstrip()
