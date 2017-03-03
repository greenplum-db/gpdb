import fnmatch
import os,sys
import tinctest
from mpp.lib.PSQL import PSQL
from partitioning.partitionindexes import PartitionIndexesSQLTestCase

class PartitionIndexesTests(PartitionIndexesSQLTestCase):
    '''
    These tests are designed to validate that index scans are being used and the SQL returns correct results
    @tags ORCA
    '''
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

class PartitionIndexesiNotSupportedTests(PartitionIndexesSQLTestCase):
    '''
    @skip Tests related to unsupported features
    '''
    sql_dir = 'notsupportedsql/'
    ans_dir = 'expected/'
    out_dir = 'output/'
