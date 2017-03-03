import os
import sys
import unittest2 as unittest

import tinctest
from fnmatch import fnmatch
from mpp.models import SQLTestCase
from tinctest.runner import TINCTextTestResult
from tinctest.lib import run_shell_command, Gpdiff
from mpp.lib.PSQL import PSQL


class DMLSQLTestCase(SQLTestCase):
    def __init__(self, methodName, baseline_result = None, sql_file = None, db_name = None):
        super(DMLSQLTestCase, self).__init__(methodName, baseline_result, sql_file, db_name)
        self.execute_all_plans = self._metadata.get('execute_all_plans', 'false')

    def _infer_metadata(self):
        super(DMLSQLTestCase, self)._infer_metadata()

    def run_test(self):

        source_dir = self.get_source_dir()
        out_dir = self.get_out_dir()
        sql_dir = os.path.join(source_dir, self.sql_dir)
        setup_file = os.path.join(sql_dir, os.path.basename(self.sql_file).replace('.sql', '_setup.sql'))
        setup_out_file = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_setup.out'))

        if self.execute_all_plans == 'True':     
            dmloperation = ''

            if 'insert' in self.sql_file.lower():
                dmloperation = 'INSERT INTO'
            elif 'update' in self.sql_file.lower():
                dmloperation = 'UPDATE'
            elif 'delete' in self.sql_file.lower():
                dmloperation ='DELETE FROM'

            # Get the entire contents of the SQL file
            query = ''
            fil = open(self.sql_file,'r')
            query = fil.read()
            fil.close()

            # Grep the DML in order to find # possible plans
            syscmd = "grep -i \"^" + dmloperation + '\" ' + self.sql_file
            p = os.popen(syscmd, 'r')
            explainquery = p.readline().strip()
            explainquery = 'EXPLAIN ' + explainquery
            
            sqlcontent = [
            "-- start_ignore",
            "set client_min_messages=\'log\';",
            "set optimizer_enumerate_plans=on;",
            "set optimizer=on;",
            "-- end_ignore",
            explainquery ]

            # Get the number of alternative plans from explain plan
            explain_file = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_explain.sql'))
            explain_file_out = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_explain.out'))
            self._add_tofile(explain_file, sqlcontent, 0)

            PSQL.run_sql_file(explain_file, dbname = self.db_name, out_file = explain_file_out)

            noplanflag = 0
            result_str = ''
            syscmd = "grep \'Number of plan alternatives:\' " + explain_file_out + " |sed -e \'s/^.*\[OPT\]:.*alternatives: //\'"
            q = os.popen(syscmd, "r")
            y = q.readline()
            q.close()
            try:
                actual_plan_id = int(y.strip())
            except ValueError:
                tinctest.logger.info('No plan generated. Check output file for a crash')
                print " No plan generated "
                result_str = 'No plan generated for the dml operation \"(%s)\" in %s.\n Please check %s for the explain plan.' %(dmloperation,self.sql_file,explain_file)
		tinctest.logger.info(result_str)
                noplanflag = 1

            if actual_plan_id == 0:
                # Query Crashes.
                noplanflag = 1
                result_str = 'Number of plan alternatives for the dml operation (%s) in %s = %s.\n Please check %s for the explain plan.' %(dmloperation,self.sql_file,actual_plan_id,explain_file)
                tinctest.logger.info('Number of plan alternatives: %s' %(actual_plan_id))
            # Number of plan generated is >= 1 
            if not(noplanflag):
                planid = 1
                tmp_result = []
                # Run the SQL file for the different plan ids
                while planid <= actual_plan_id :
                    sql_file_planid = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_planid_' +str(planid)+ '.sql'))
                    syscmd = "rm " + sql_file_planid
                    run_shell_command(syscmd, "Delete any existing planid files")
                    out_file_planid = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_planid_' +str(planid)+ '.out'))
                    setup_outfile_planid = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_setup_' +str(planid)+ '.out'))
                    sqlcontent = [
                    "--start_ignore",
                    "set optimizer=on;",
                    "set optimizer_enumerate_plans=on;",
                    "set optimizer_plan_id= " +str(planid)+ ";",
                    "--end_ignore",
                    query ]
                    self._add_tofile(sql_file_planid, sqlcontent, 0)
    
                    PSQL.run_sql_file(setup_file, dbname = self.db_name, out_file = setup_outfile_planid)
                    PSQL.run_sql_file(sql_file_planid, dbname = self.db_name, out_file = out_file_planid)

                    local_result = Gpdiff.are_files_equal(out_file_planid, self.ans_file)
                    if local_result == False:
                        return False
    
                    tmp_result.append(Gpdiff.are_files_equal(out_file_planid, self.ans_file ))
                    planid += 1
    
                for r in tmp_result:
                    if r == False:
                        return False
                return True
    
            else:
                #create a failure.diff file that states that the plan was not produced
                self.resultobj = False
                extra_diff_file = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_extra_failure.diff'))
                cmd = "echo \"" + result_str+ "\" > " + extra_diff_file
                run_shell_command(cmd, "Adding failure to extra diff file")
                return False
        else:
            #Run the sql file and get the output as SQLTestCase
            return super(DMLSQLTestCase, self).run_test()



    def _add_tofile(self, filename, lines, pos):
        """
        @param filename we're adding line to
        @param line to add to the file
        @param pos the line number at which the line is added\n
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

