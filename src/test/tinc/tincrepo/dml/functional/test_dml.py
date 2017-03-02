from mpp.models import SQLTestCase
import os

'''
Test DML Queries for ORCA
'''
class test_dml(SQLTestCase):
    """
    @optimizer_mode on
    @tags ORCA
    @db_name dmldb
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

    def get_substitutions(self):
        MYD = os.path.dirname(os.path.realpath(__file__))
        substitutions = { '%MYD%' : MYD }
        return substitutions

class test_dml_partition(SQLTestCase):
    """
    @optimizer_mode on
    @product_version gpdb: 4.3.99.99, gpdb: [4.3.4.0-]
    @tags ORCA MPP-21090
    @db_name dmldb
    """
    sql_dir = 'sql_partition/'
    ans_dir = 'expected_partition/'
    out_dir = 'output/'
