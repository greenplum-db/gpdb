import os
import sys

from mpp.models import SQLTestCase
from gppylib.commands.base import Command
from query.indexapply import IndexApplyTestCase

MYD = os.path.dirname(os.path.realpath(__file__))

### tests for index created on non-distributed key
### Index created on non-distribution key:
### (X is distributed on X.i), (Y is distributed on Y.i), (Y has index on Y.j)
class Bfv_IndexApply_index_on_nondist_int(IndexApplyTestCase):
    """
    @db_name bfv_index_on_nondist_int
    @product_version gpdb: [4.3-]
    @tags ORCA
    @gpopt 1.522
    """
    sql_dir = 'sql_bfv/'
    ans_dir = 'expected_bfv/'
    out_dir = 'output_bfv_index_on_nondist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}
