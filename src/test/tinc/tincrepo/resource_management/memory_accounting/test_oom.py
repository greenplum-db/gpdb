import tinctest
import unittest2 as unittest

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL

from gppylib.commands.base import Command
from tinctest.models.scenario import ScenarioTestCase

class OOMTestCase(MPPTestCase, ScenarioTestCase):
    """
    @product_version gpdb:[4.3.0.0-MAIN], hawq: [1.2.1.0-]
    """
    @classmethod
    def tearDownClass(cls):
        # Reset GUC gp_vmem_protect_limit to default
        Command('Run gpconfig to set GUC gp_vmem_protect_limit' ,
                'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -m 8192 -v 8192; gpconfig -c gp_vmem_limit_per_query -v 0 --skipvalidation').run(validateAfter=True)

        # Restart DB
        Command('Restart database for GUCs to take effect',
                'source $GPHOME/greenplum_path.sh && gpstop -ar').run(validateAfter=True)

    def gp_version(self):
        """
        @todo: ScenarioTest does not have product from MPPTestCase, need to have the method in ScenarioTestCase.
        This is only a hack.
        """
        result = PSQL.run_sql_command( sql_cmd='select version()', flags='-t -q' )
        if "HAWQ" in result:
            return "hawq"
        else:
            return "gpdb"

    def setUp(self):
        # Set GUC gp_vmem_protect_limit
        self.prd = "_hawq"
        if self.gp_version() == "gpdb":
            self.prd = ""

        tinctest.logger.info('Setting GUC and restarting DB')

        if self.name == 'OOMTestCase.test_06_OOM_massivequery':
            vmem = 40 # Set vmem_protect for GPDB
            Command('Run gpconfig to set GUC gp_vmem_protect_limit' ,
                    'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -m %s -v %s' % (vmem, vmem)).run(validateAfter=True)
        elif self.name == 'OOMTestCase.test_07_OOM_abort_query':
            Command('Run gpconfig to set GUC gp_vmem_protect_limit' ,
                    'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_limit_per_query -v "4MB" --skipvalidation').run(validateAfter=True)
        elif self.name == 'OOMTestCase.test_05_dumpusage':
            Command('Run gpconfig to set GUC gp_vmem_protect_limit' ,
                    'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -m 4 -v 4').run(validateAfter=True)
        else:
            Command('Run gpconfig to set GUC gp_vmem_protect_limit' ,
                    'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -m 20 -v 20').run(validateAfter=True)

        # Restart DB
        Command('Restart database for GUCs to take effect',
                'source $GPHOME/greenplum_path.sh && gpstop -ar').run(validateAfter=True)

        super(OOMTestCase, self).setUp()

    def test_01_OOM_with_singlequery(self):
        """
        @description Run a single query OOM and verify log
        """
        test_case_list1 = []
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_singlequery_oom')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_verify_singlequery_oom%s' % self.prd)
        self.test_case_scenario.append(test_case_list2)

    def test_02_OOM_concurrent_sleeps(self):
        """
        @description Run a single query OOM while multiple other queries are sleeping and verify log
        """
        test_case_list1 = []
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_sleep')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_sleep')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom')
        self.test_case_scenario.append(test_case_list1)
         

        test_case_list2 = []
        test_case_list2.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_verify_concurrent_sleeps_oom%s' %self.prd)
        self.test_case_scenario.append(test_case_list2)

    def test_03_OOM_multiple_random(self):
        """
        @description Test where multiple active queries randomly hit OOM
        """
        test_case_list1 = []
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_mixed_1')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_mixed_2')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_mixed_2')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_mixed_2')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_simple')
        self.test_case_scenario.append(test_case_list1)


        test_case_list2 = []
        test_case_list2.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_verify_multiple_random_oom%s' % self.prd)
        self.test_case_scenario.append(test_case_list2)

    # skipping the test for 1.3.1.0 since it takes hours to run this test
    # def test_04_multipleslice_singlequery(self):
    #     """
    #     @description Test where single query with multiple slices per segment runs OOM
    #     """
    #     test_case_list1 = []
    #     test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_multislice_oom')
    #     self.test_case_scenario.append(test_case_list1)
    #
    #     test_case_list2 = []
    #     test_case_list2.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_verify_multislice_oom%s' % self.prd)
    #     self.test_case_scenario.append(test_case_list2)
    #
    #     test_case_list3 = []
    #     test_case_list3.append('resource_management.memory_accounting.scenario.oom_test.runsql.verify.test_oom_count')
    #     self.test_case_scenario.append(test_case_list3)

    def test_05_dumpusage(self):
        """
        @description Test with GUC gp_dump_memory_usage
        """
        test_case_list1 = []
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_dumpusage')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('resource_management.memory_accounting.scenario.oom_test.runsql.verify.test_dumpusage')
        self.test_case_scenario.append(test_case_list2)

    # QA-2748, need at least 48GB ram to run this test
    # GPDB should use the DCA, for HAWQ should use gpdb26.rel.dh.greenplum.com
    # This test is dependent on how much memory
    @unittest.skip("QA-2748, issue with test on different platform with different memory")
    def test_06_OOM_massivequery(self):
        """
        @description Test where smaller queries pass while the massive violator dies
        """
        test_case_list1 = []
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_small')
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_oom_massive')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_verify_massive_oom%s' % self.prd)
        self.test_case_scenario.append(test_case_list2)

    def test_07_OOM_abort_query(self):
        """
        @description Need a mechanism to abort query before gp_vmem_protect_limit is hit
        @note Depending on the machine, we may get "VM Protect failed to allocate memory"
              or "Per-query VM protect limit reached: current limit is 102400 kB, requested 8388608 bytes, available 2 MB"
        """
        test_case_list1 = []
        test_case_list1.append('resource_management.memory_accounting.scenario.oom_test.runsql.runtest.test_verify_oom_abort_query')
        self.test_case_scenario.append(test_case_list1)
