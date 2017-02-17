from resource_management.runaway_query.runaway_query_scenario import \
    RQTBaseScenarioTestCase, RQTBaseSQLTestCase
from gppylib.commands.base import Command
import datetime, os, random, shutil, tinctest

class RQTScenarioTestCase(RQTBaseScenarioTestCase):
    """
    @concurrency 20
    @tags runaway_query_termination
    @rqt_sql_test_case resource_management.runaway_query.runaway_query_scenario.test_runaway_query_scenario.RQTSQLTestCase
    """

class RQTSQLTestCase(RQTBaseSQLTestCase):
    """
    @db_name runaway_query
    @vlimMB 1024
    @redzone 10
    @iterations 20
    """
    sql_dir = 'sql/'
    out_dir = 'output/'
    
    def test_run(self):
        """
        For each iteration, pick a sql file and run. 
        """
        sql_files = os.listdir(self.get_sql_dir())
        for iter in range(1, self.iterations+1):
            sql_file = self.get_next_sql(sql_files, iter)
            current_timestamp = datetime.datetime.now().strftime('%Y-%m-%d-%H-%M-%S-%f')
            savepoint_name = current_timestamp.replace('-','')
            run_sql_file = os.path.join(self.get_out_dir(), '%s-iter%s-%s'%(current_timestamp, iter, sql_file))
            # need to replace SAVEPOINT_NAME with timestamp
            with open(run_sql_file, 'w') as o:
                f = open(os.path.join(self.get_sql_dir(),sql_file), 'r')
                text = f.read()
                o.write(text.replace('SAVEPOINT_NAME', savepoint_name))  
            run_out_file = run_sql_file.replace('.sql','.out')
            run_sql_cmd = 'psql -d %s -a -f %s &> %s'%(self.db_name, run_sql_file, run_out_file)
            Command('Run Sql File', run_sql_cmd).run(validateAfter=True)     
            end_timestamp = datetime.datetime.now().strftime('%Y-%m-%d-%H-%M-%S-%f')
            with open(run_out_file, 'a') as o:
                o.write('\nFINISH_TIME:%s'%end_timestamp)
