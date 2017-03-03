import os
import sys
from gppylib.commands.base import Command
from query.indexapply import IndexApplyTestCase

MYD = os.path.dirname(os.path.realpath(__file__))




### tests for index created on non-distributed key
### Index created on non-distribution key:
### (X is distributed on X.i), (Y is distributed on Y.i), (Y has index on Y.j)
class IndexApply_index_on_nondist_int(IndexApplyTestCase):
    """
    @db_name index_on_nondist_int
    @product_version gpdb: [4.3-] 
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_index_on_nondist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_index_on_nondist_text(IndexApplyTestCase):
    """
    @db_name index_on_nondist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_index_on_nondist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_index_on_nondist_varchar(IndexApplyTestCase):
    """
    @db_name index_on_nondist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_index_on_nondist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

### tests for index created on distributed keys
### Index created on distribution key:
### (X is distributed on X.i), (Y is distributed on Y.j), (Y has index on Y.j)
class IndexApply_index_on_dist_int(IndexApplyTestCase):
    """
    @db_name index_on_dist_int
    @product_version gpdb: [4.3-] 
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_index_on_dist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i', '%yDIST%':'j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_index_on_dist_text(IndexApplyTestCase):
    """
    @db_name index_on_dist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_index_on_dist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i', '%yDIST%':'j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_index_on_dist_varchar(IndexApplyTestCase):
    """
    @db_name index_on_dist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_index_on_dist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i', '%yDIST%':'j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

### Multiple distribution keys in outer side:
### (X is distributed on X.i,X.j), (Y is distributed on Y.i), (Y has index on Y.j)
class IndexApply_MultiDist_outer_int(IndexApplyTestCase):
    """
    @db_name multidist_outer_int
    @product_version gpdb: [4.3-] 
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_MultiDist_outer_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_MultiDist_outer_text(IndexApplyTestCase):
    """
    @db_name multidist_outer_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_MultiDist_outer_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_MultiDist_outer_varchar(IndexApplyTestCase):
    """
    @db_name multidist_outer_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_MultiDist_outer_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}


### Multiple distribution keys in inner side:
### (X is distributed on X.i), (Y is distributed on Y.i, Y.j), (Y has index on Y.j)
class IndexApply_multi_dist_inner_int(IndexApplyTestCase):
    """
    @db_name multi_dist_inner_int
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_multi_dist_inner_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_multi_dist_inner_text(IndexApplyTestCase):
    """
    @db_name multi_dist_inner_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_multi_dist_inner_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_multi_dist_inner_varchar(IndexApplyTestCase):
    """
    @db_name multi_dist_inner_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_multi_dist_inner_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
### 
### Multiple distribution keys in both sides:
### (X is distributed on X.i,X.j), (Y is distributed on Y.i, Y.j), (Y has index on Y.j)
class IndexApply_multi_dist_both_int(IndexApplyTestCase):
    """
    @db_name multi_dist_both_int
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_multi_dist_both_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_multi_dist_both_text(IndexApplyTestCase):
    """
    @db_name multi_dist_both_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_multi_dist_both_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_multi_dist_both_varchar(IndexApplyTestCase):
    """
    @db_name multi_dist_both_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_multi_dist_both_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
### 
### Multi-Column Index on non-distribution keys:
### (X is distributed on X.i,X.j), (Y is distributed on Y.i), (Y has index on Y.j,Y.k)
class IndexApply_multi_index_nondist_int(IndexApplyTestCase):
    """
    @db_name multi_index_nondist_int
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_multi_index_nondist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j,k', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_multi_index_nondist_text(IndexApplyTestCase):
    """
    @db_name multi_index_nondist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_multi_index_nondist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j,k', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_multi_index_nondist_varchar(IndexApplyTestCase):
    """
    @db_name multi_index_nondist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_multi_index_nondist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j,k', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
### 
### Multi-Column Index on distribution keys:
### (X is distributed on X.i,X.j), (Y is distributed on Y.i,Y.j), (Y has index on Y.i,Y.j)
class IndexApply_multi_index_ondist_int(IndexApplyTestCase):
    """
    @db_name multi_index_ondist_int
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_multi_index_ondist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_multi_index_ondist_text(IndexApplyTestCase):
    """
    @db_name multi_index_ondist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_multi_index_ondist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_multi_index_ondist_varchar(IndexApplyTestCase):
    """
    @db_name multi_index_ondist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_multi_index_ondist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'i,j', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
### 
### Multi-Column Index intersecting distribution keys:
### (X is distributed on X.i,X.j), (Y is distributed on Y.i,Y.k), (Y has index on Y.i,Y.j)
class IndexApply_multi_index_intersect_dist_int(IndexApplyTestCase):
    """
    @db_name multi_index_intersect_dist_int
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_multi_index_intersect_dist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'i,k', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_multi_index_intersect_dist_text(IndexApplyTestCase):
    """
    @db_name multi_index_intersect_dist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_multi_index_intersect_dist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'i,k', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_multi_index_intersect_dist_varchar(IndexApplyTestCase):
    """
    @db_name multi_index_intersect_dist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_multi_index_intersect_dist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'i,k', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
### 
### Multi-Column Index disjoint of distribution keys:
### (X is distributed on X.i,X.j), (Y is distributed on Y.k,Y.m), (Y has index on Y.i,Y.j)
class IndexApply_multi_index_disjoint_dist_int(IndexApplyTestCase):
    """
    @db_name multi_index_disjoint_dist_int
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_multi_index_disjoint_dist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'k,m', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_multi_index_disjoint_dist_text(IndexApplyTestCase):
    """
    @db_name multi_index_disjoint_dist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_multi_index_disjoint_dist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'k,m', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_multi_index_disjoint_dist_varchar(IndexApplyTestCase):
    """
    @db_name multi_index_disjoint_dist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_multi_index_disjoint_dist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'k,m', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
### 
### Index condition matches outer distribution
### (X is distributed on X.j), (Y is distributed on Y.j), (Y has index on Y.j)
class IndexApply_idxcond_match_outerdist_int(IndexApplyTestCase):
    """ 
    @db_name idxcond_match_outerdist_int
    @product_version gpdb: [4.3-] 
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_idxcond_match_outerdist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'j', '%yDIST%':'j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}


class IndexApply_idxcond_match_outerdist_text(IndexApplyTestCase):
    """ 
    @db_name idxcond_match_outerdist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_idxcond_match_outerdist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'j', '%yDIST%':'j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}

class IndexApply_idxcond_match_outerdist_varchar(IndexApplyTestCase):
    """ 
    @db_name idxcond_match_outerdist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_idxcond_match_outerdist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'j', '%yDIST%':'j', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
                                                       
### 
### Index condition is subset of outer distribution
### (X is distributed on X.i,X.j), (Y is distributed on Y.i), (Y has index on Y.j)
class IndexApply_idxcond_subset_outer_dist_int(IndexApplyTestCase):
    """ 
    @db_name idxcond_subset_outer_dist_int
    @product_version gpdb: [4.3-] 
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_idxcond_subset_outer_dist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}
 
 
class IndexApply_idxcond_subset_outer_dist_text(IndexApplyTestCase):
    """ 
    @db_name idxcond_subset_outer_dist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_idxcond_subset_outer_dist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
 
class IndexApply_idxcond_subset_outer_dist_varchar(IndexApplyTestCase):
    """ 
    @db_name idxcond_subset_outer_dist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_idxcond_subset_outer_dist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i,j', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
 

### 
### Index condition is superset of outer distribution
### (X is distributed on X.i), (Y is distributed on Y.i), (Y has index on Y.i,Y.j)
class IndexApply_idxcond_superset_outer_dist_int(IndexApplyTestCase):
    """ 
    @db_name idxcond_superset_outer_dist_int
    @product_version gpdb: [4.3-] 
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_int/'
    out_dir = 'output_idxcond_superset_outer_dist_int/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%':'int', '%xDIST%':'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'+', '%xPART%':'', '%yPART%':''}
 
 
class IndexApply_idxcond_superset_outer_dist_text(IndexApplyTestCase):
    """ 
    @db_name idxcond_superset_outer_dist_text
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_text/'
    out_dir = 'output_idxcond_superset_outer_dist_text/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'text', '%xDIST%' : 'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
 
class IndexApply_idxcond_superset_outer_dist_varchar(IndexApplyTestCase):
    """ 
    @db_name idxcond_superset_outer_dist_varchar
    @product_version gpdb: [4.3-]
    @tags ORCA FEATURE_BRANCH_ONLY
    @gpopt 1.493
    """
    sql_dir = 'sql/'
    ans_dir = 'expected_varchar/'
    out_dir = 'output_idxcond_superset_outer_dist_varchar/'
    template_dir = 'template'
    template_subs = {'%MYD%':MYD, '%idxtype%':'btree', '%datatype%' : 'varchar', '%xDIST%' : 'i', '%yDIST%':'i', '%xIDX%':'NONE' , '%yIDX%':'i,j', '%ADD%':'||', '%xPART%':'', '%yPART%':''}
 

