import os
from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL

class runtest(SQLTestCase):
    """
    @db_name memory_accounting
    """
    sql_dir = 'sql/'
    ans_dir = 'answer/'
    out_dir = 'output/'

class verify(SQLTestCase):

    def gp_version(self):
        result = PSQL.run_sql_command( sql_cmd='select version()', flags='-t -q' )
        if "HAWQ" in result:
            return "hawq"
        else:
            return "gpdb"
    
    def test_oom_count(self):
        prd = "hawq"
        if self.gp_version() == "gpdb":
            prd = "gp"
        # SQL command to find number of processes logging memory usage for single segment seg0 when a 4-slice query runs OOM
        search = "%Logging memory usage%"
        sql_command = """select count(*) from (select distinct logpid, logslice, logsegment from %s_toolkit.__%s_log_segment_ext where logmessage like '%s' and  logtime >= (select logtime from %s_toolkit.__%s_log_master_ext where logmessage like 'statement: select 4 as oom_test;' order by logtime desc limit 1) and logsegment='seg0' group by logpid, logslice, logsegment order by logsegment,logslice desc) as foo;""" % (prd, prd, search, prd, prd)
        result = PSQL.run_sql_command( sql_cmd='%s ' %(sql_command), flags='-t -q' )
        # Verify that OOM log count is at least the number of slices in the query
        self.failUnless(int(result) >= 4, 'OOM log count should be at least the number of slices. OOM log count is ' + result)
