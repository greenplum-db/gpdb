from mpp.models import SQLTestCase
import os

class test_partitioning_ddl(SQLTestCase):
    '''
    Includes testing the multiple aspects of creating mpartitioned tables
    '''
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

    def get_substitutions(self):
        MYD = os.path.dirname(os.path.realpath(__file__))
        substitutions = { '%MYD%' : MYD }
        return substitutions

