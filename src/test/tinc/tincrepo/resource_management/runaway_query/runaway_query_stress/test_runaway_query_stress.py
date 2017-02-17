from mpp.models.sql_concurrency_tc import SQLConcurrencyTestCase
from resource_management.runaway_query.runaway_udf import create_runaway_udf, drop_runaway_udf

class RunawayQueryStressTestCase(SQLConcurrencyTestCase):
    """
    @db_name gptest
    @tags runaway_query_termination
    @gpdiff True
    """
    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

    @classmethod
    def setUpClass(cls):
        super(RunawayQueryStressTestCase, cls).setUpClass()
        create_runaway_udf(cls.db_name)
        
    @classmethod
    def tearDownClass(cls):
        drop_runaway_udf(cls.db_name)
