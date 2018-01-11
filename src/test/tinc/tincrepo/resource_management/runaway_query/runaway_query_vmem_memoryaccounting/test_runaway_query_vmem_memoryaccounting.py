from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.lib.sql_isolation_testcase import \
    SQLIsolationTestCase
from mpp.lib.PSQL import PSQL
from resource_management.runaway_query.runaway_udf import *
import tinctest


def _set_VLIM_SLIM(vlimMB, slimMB):

    # Set up GUCs for VLIM (gp_vmem_protect_limit) and SLIM (gp_vmem_limit_per_query)
    tinctest.logger.info('Setting GUCs for VLIM gp_vmem_protect_limit=%dMB and SLIM gp_vmem_limit_per_query=%dMB' % (vlimMB, slimMB))
    Command('Run gpconfig to set GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v %d' % vlimMB).run(validateAfter=True)
    Command('Run gpconfig to set GUC gp_vmem_limit_per_query',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_limit_per_query -v %d --skipvalidation' % (slimMB * 1024)).run(validateAfter=True)

    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -air').run(validateAfter=True)

def _reset_VLIM_SLIM():

    # Reset GUCs for VLIM (gp_vmem_protect_limit) and SLIM (gp_vmem_limit_per_query)
    tinctest.logger.info('Resetting GUCs for VLIM gp_vmem_protect_limit and SLIM gp_vmem_limit_per_query')
    Command('Run gpconfig to reset GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v 8192').run(validateAfter=True)
    Command('Run gpconfig to reset GUC gp_vmem_limit_per_query',
            'source $GPHOME/greenplum_path.sh;gpconfig -r gp_vmem_limit_per_query --skipvalidation').run(validateAfter=True)
    
    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -air').run(validateAfter=True)


class RQTMemoryAccountingTestCase(SQLIsolationTestCase):
    """
    @tags runaway_query_termination
    """

    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

    def _infer_metadata(self):
        super(RQTMemoryAccountingTestCase, self)._infer_metadata()
        try:
            self.vlimMB = int(self._metadata.get('vlimMB', '8192')) # Default is 8192
            self.slimMB = int(self._metadata.get('slimMB', '0')) # Default is 0
        except Exception:
            tinctest.logger.info("Error getting the testcase related metadata")
            raise
    
    def setUp(self):
        _set_VLIM_SLIM(self.vlimMB, self.slimMB)
        return super(RQTMemoryAccountingTestCase, self).setUp()

    @classmethod
    def setUpClass(cls):
        super(RQTMemoryAccountingTestCase, cls).setUpClass()
        create_runaway_udf()
        create_session_state_view()
        
    @classmethod
    def tearDownClass(cls):
        drop_session_state_view()
        drop_runaway_udf()
        _reset_VLIM_SLIM()
 
    def verify_out_file(self, out_file, ans_file):
        """
        Override SQLTestCase, verify vmem shown in the live view matched the memory accounting output in explain analyze
        """
        lines = [line.strip() for line in open(out_file)]
        vmem_mb = 0
        mem_memory_accounting = 0.0
        for num in range(len(lines)):
            cur_line = lines[num]
            if cur_line.startswith('vmem_mb'):
                try:
                    vmem_mb = int(lines[num+2])
                except:
                    raise
            elif cur_line.startswith('(slice') and line.find('workers,') != -1:
                # slices that happen in segments
                temp_line = cur_line[cur_line.index('Peak memory:'):]
                try:
                    # the line looks like: Peak memory: 1585K bytes.
                    peak_mem_str = temp_line.split(' ')[2]
                    peak_mem_max = int(peak_mem_str[:-1])
                    print peak_mem_max
                except:
                    raise
                mem_memory_accounting += peak_mem_max
        
        # Comparing vmem_mb with the memory recorded in memory accounting
        # TODO 2014-10-01 Zhongxian.Gu
        # Calibrate difference MARGIN
        MARGIN = 1
        mem_memory_accounting /= 1024.0 
        if vmem_mb - MARGIN <= mem_memory_accounting and mem_memory_accounting <= vmem_mb + MARGIN:
            return True
        
        return False
    
