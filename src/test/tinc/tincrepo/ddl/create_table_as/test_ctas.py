import os

from mpp.models import SQLTestCase
from mpp.lib.config import GPDBConfig
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.gpfilespace import HAWQGpfilespace

def setUpFilespaceForCTAS(isForHawq):
      config = GPDBConfig()
      if isForHawq:
            filespace = HAWQGpfilespace()
      else:
            filespace = Gpfilespace()
      if config.is_not_insync_segments():
             filespace.create_filespace('tincrepo_qp_ddl_ctas')


class TestCTASWithOrcaInGPDB(SQLTestCase):
       '''
       testing create table as with Orca. optimizer=on is added in gucs
       because the _setup files ignore the optimizer_mode parameter
       @optimizer_mode on
       @gucs optimizer_enable_ctas=on; optimizer_log=on; gp_create_table_random_default_distribution=on; optimizer=on
       @product_version gpdb: [4.3.3-]
       '''
       sql_dir = 'sql/'
       ans_dir = 'expected/'
       out_dir = 'output_orca/'

       @classmethod
       def setUpClass(cls):
              super(TestCTASWithOrcaInGPDB, cls).setUpClass()
              setUpFilespaceForCTAS(False)

       @classmethod
       def get_substitutions(self):
             MYD = os.path.dirname(os.path.realpath(__file__))
             substitutions = { '%MYD%' : MYD, '%USED_OPT%' : 'orca' }
             return substitutions


class TestCTASWithPlannerInGPDB(SQLTestCase):
       '''
       testing create table as with Planner. optimizer=off is added in gucs
       because the _setup files ignore the optimizer_mode parameter
       @optimizer_mode off
       @gucs gp_create_table_random_default_distribution=on; optimizer=off
       @product_version gpdb: [4.3.3-]
       '''
       sql_dir = 'sql/'
       ans_dir = 'expected/'
       out_dir = 'output_planner/'

       @classmethod
       def setUpClass(cls):
              super(TestCTASWithPlannerInGPDB, cls).setUpClass()
              setUpFilespaceForCTAS(False)

       @classmethod
       def get_substitutions(self):
             MYD = os.path.dirname(os.path.realpath(__file__))
             substitutions = { '%MYD%' : MYD, '%USED_OPT%' : 'planner' }
             return substitutions
