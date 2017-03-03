import tinctest

from mpp.models import SQLTestCase
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL

def _set_VLIM(vlimMB):

    # Set up GUCs for VLIM (gp_vmem_protect_limit), SLIM (gp_vmem_limit_per_query) and RQT activation percent (runaway_detector_activation_percent)
    tinctest.logger.info('Setting GUCs for VLIM gp_vmem_protect_limit=%dMB'%(vlimMB))
    Command('Run gpconfig to set GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v %d' % vlimMB).run(validateAfter=True)

    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -ar').run(validateAfter=True)

def _reset_VLIM ():

    # Reset GUCs for VLIM (gp_vmem_protect_limit), SLIM (gp_vmem_limit_per_query) and RQT activation percent (runaway_detector_activation_percent)
    tinctest.logger.info('Resetting GUCs for VLIM gp_vmem_protect_limit')
    Command('Run gpconfig to reset GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v 8192').run(validateAfter=True)
    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -ar').run(validateAfter=True)


class TooManyExecAccountsTestCase(SQLTestCase):
    """
    @tags memory_accounting
    """

    '''
    SQL Test for Too many executor accounts in memory accounting
    '''
        
    def _infer_metadata(self):
        super(TooManyExecAccountsTestCase, self)._infer_metadata()
        try:
            self.vlimMB = int(self._metadata.get('vlimMB', '8192')) # Default is 8192
        except Exception:
            tinctest.logger.info("Error getting the testcase related metadata")
            raise

    def setUp(self):
        _set_VLIM(self.vlimMB)
        return super(TooManyExecAccountsTestCase, self).setUp()

    @classmethod
    def setUpClass(cls):
        super(TooManyExecAccountsTestCase, cls).setUpClass()

    @classmethod
    def tearDownClass(cls):
        _reset_VLIM()
        
    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'
        
