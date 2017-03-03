from mpp.models import SQLTestCase
import os

class test_triggers(SQLTestCase):
    '''
    Includes testing for dml using triggers with Orca on (MPP-24827, GPSQL-2934)
    @author elhela
    @created 2014-10-22 12:00:00
    @modified 2014-10-22 12:00:00
    @description Tests for DML with triggers (MPP-24827, GPSQL-2934)
    @tags MPP-24827 GPSQL-2934 ORCA HAWQ
    @product_version gpdb: [4.3.3.0-]
    @optimizer_mode on
    @gpopt 1.504
    '''
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

    @classmethod
    def get_substitutions(self):
        MYD = os.path.dirname(os.path.realpath(__file__))
        substitutions = { '%MYD%' : MYD}
        return substitutions
