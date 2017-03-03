from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL

from tinctest.lib import run_shell_command, Gpdiff
import tinctest
import os,sys

class CTEtests_inlining_disabled(SQLTestCase):
    """
    @optimizer_mode on
    @tags ORCA
    @db_name world_db
    @gucs optimizer_cte_inlining = off
    """
    sql_dir = 'sql/'
    ans_dir = 'answer/'
    out_dir = 'output_inlining_disabled/'
    
    def __init__(self, methodName, baseline_result = None, sql_file = None, db_name = None):
        super(CTEtests_inlining_disabled,self).__init__(methodName, baseline_result, sql_file, db_name)
        self.checkplan = self._metadata.get('checkplan', 'false')

    def verify_out_file(self, out_file, ans_file):
	# MPP-22085: Enabling CTE inlining reduces plan space 
        if self.checkplan == 'True':
	    try:
                f = open(out_file,'r')
                out = f.read()
                f.close()
                import re
                m = re.findall('Number of plan alternatives: \d+',out)
	        altplans = []
                for i in m:
                    altplans.append(re.search('\d+',i).group(0))
                return int(altplans[1]) > int(altplans[0])
	    except:
                tinctest.logger.info('Check the outfile %s for \"Number of plan alternatives\" '%out_file)
	        return False
        return super(CTEtests_inlining_disabled, self).verify_out_file(out_file, ans_file)

class CTEtests_inlining_enabled(SQLTestCase):
    """
    @optimizer_mode on
    @db_name world_db
    @tags ORCA
    @gucs optimizer_cte_inlining = on;optimizer_cte_inlining_bound=1000
    """
    sql_dir = 'sql/'
    ans_dir = 'answer/'
    out_dir = 'output_inlining_enabled/'

class CTEtests_notsupported(SQLTestCase):
    """
    @optimizer_mode on
    @db_name world_db
    @tags ORCA
    """
    sql_dir = 'sql_others/'
    ans_dir = 'answer/'
    out_dir = 'output/' 
