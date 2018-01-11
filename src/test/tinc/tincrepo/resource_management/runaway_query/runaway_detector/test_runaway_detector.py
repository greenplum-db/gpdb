import tinctest

from mpp.gpdb.tests.storage.lib.sql_isolation_testcase import SQLIsolationTestCase
from gppylib.commands.base import Command
from resource_management.runaway_query.runaway_udf import *
from mpp.lib.PSQL import PSQL

def _set_VLIM_SLIM_REDZONEPERCENT(vlimMB, slimMB, activationPercent):

    # Set up GUCs for VLIM (gp_vmem_protect_limit), SLIM (gp_vmem_limit_per_query) and RQT activation percent (runaway_detector_activation_percent)
    tinctest.logger.info('Setting GUCs for VLIM gp_vmem_protect_limit=%dMB, SLIM gp_vmem_limit_per_query=%dMB and RQT activation percent runaway_detector_activation_percent=%s'%(vlimMB, slimMB, activationPercent))
    Command('Run gpconfig to set GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v %d' % vlimMB).run(validateAfter=True)
    Command('Run gpconfig to set GUC gp_vmem_limit_per_query',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_limit_per_query -v %d --skipvalidation' % (slimMB * 1024)).run(validateAfter=True)
    Command('Run gpconfig to set GUC runaway_detector_activation_percent',
            'source $GPHOME/greenplum_path.sh;gpconfig -c runaway_detector_activation_percent -v %d --skipvalidation' % activationPercent).run(validateAfter=True)

    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -air').run(validateAfter=True)

def _reset_VLIM_SLIM_REDZONEPERCENT():

    # Reset GUCs for VLIM (gp_vmem_protect_limit), SLIM (gp_vmem_limit_per_query) and RQT activation percent (runaway_detector_activation_percent)
    tinctest.logger.info('Resetting GUCs for VLIM gp_vmem_protect_limit, SLIM gp_vmem_limit_per_query, and RQT activation percent runaway_detector_activation_percent')
    Command('Run gpconfig to reset GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v 8192').run(validateAfter=True)
    Command('Run gpconfig to reset GUC gp_vmem_limit_per_query',
            'source $GPHOME/greenplum_path.sh;gpconfig -r gp_vmem_limit_per_query --skipvalidation').run(validateAfter=True)
    Command('Run gpconfig to reset GUC runaway_detector_activation_percent',
            'source $GPHOME/greenplum_path.sh;gpconfig -r runaway_detector_activation_percent --skipvalidation').run(validateAfter=True)
    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -air').run(validateAfter=True)

class RunawayDetectorTestCase(SQLIsolationTestCase):
    """
    @tags runaway_query_termination
    """

    '''
    Test for Runaway Query Termination that require concurrent sessions
    '''

    def _infer_metadata(self):
        super(RunawayDetectorTestCase, self)._infer_metadata()
        try:
            self.vlimMB = int(self._metadata.get('vlimMB', '8192')) # Default is 8192
            self.slimMB = int(self._metadata.get('slimMB', '0')) # Default is 0
            self.activationPercent = int(self._metadata.get('redzone', '80')) # Default is 80
        except Exception:
            tinctest.logger.info("Error getting the testcase related metadata")
            raise
        
    def faultInjector(self, faultIdentifier, faultType, segId, sleepTime=10, numOccurences=1):
        tinctest.logger.info('Injecting fault: id=%s, fault=%s, segId=%d' %
            (faultIdentifier, faultType, segId))
        finjectCmd = 'source $GPHOME/greenplum_path.sh; '\
                                  'gpfaultinjector -f %s '\
                                  '-y %s --seg_dbid %d ' \
                                  '--sleep_time_s=%d '\
                                  '-o %d ' % (faultIdentifier, faultType, segId, sleepTime, numOccurences)
        tinctest.logger.info('Fault injector command: ' + finjectCmd)
        gpfaultinjector = Command('fault injector', finjectCmd)
        gpfaultinjector.run()

    def setUp(self):
        _set_VLIM_SLIM_REDZONEPERCENT(self.vlimMB, self.slimMB, self.activationPercent)
        # segid = 2, sleepTime = 20, numOccurences = 0 
        # numOccurences = 0 means we'll keep triggering the fault until we reset it
        self.faultInjector('runaway_cleanup', 'sleep', 2, 20, 0)
        return super(RunawayDetectorTestCase, self).setUp()

    def tearDown(self):
        self.faultInjector('runaway_cleanup', 'reset', 2)        
        return super(RunawayDetectorTestCase, self).tearDown()

    @classmethod
    def setUpClass(cls):
        super(RunawayDetectorTestCase, cls).setUpClass()
        create_runaway_udf()
        create_session_state_view()
        
    @classmethod
    def tearDownClass(cls):
        drop_session_state_view()
        drop_runaway_udf()
        _reset_VLIM_SLIM_REDZONEPERCENT()
 
    
    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

