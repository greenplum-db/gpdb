from mpp.models import SQLTestCase

class test_create_table_default_distribution(SQLTestCase):
       '''
       testing CREATE TABLE with default distribution
       @author garcic12
       @created 2014-07-01 18:09:00
       @description test CREATE TABLE with default distribution
       @gpopt 1.510
       @gucs gp_create_table_random_default_distribution=on
       @tags CTAS HAWQ ORCA
       @product_version gpdb: 4.3.99.99, [4.3-], 4.3.2.0ORCA1, hawq: [1.3-]
       '''

       sql_dir = 'sql/'
       ans_dir = 'expected/'
       out_dir = 'out/'

class test_create_table_default_distribution_GPDB(SQLTestCase):
       '''
       testing CREATE TABLE with default distribution
       @author garcic12
       @created 2014-07-01 18:09:00
       @description test CREATE TABLE with default distribution
       @gpopt 1.510
       @gucs gp_create_table_random_default_distribution=on
       @tags CTAS ORCA
       @product_version gpdb: 4.3.99.99, [4.3-], 4.3.2.0ORCA1
       '''

       sql_dir = 'sql_gpdb/'
       ans_dir = 'expected/'
       out_dir = 'out/'

class test_create_table_default_distribution_On(SQLTestCase):
       '''
       testing CREATE TABLE with default distribution
       @author garcic12
       @created 2014-07-01 18:09:00
       @tags CTAS HAWQ ORCA
       @product_version gpdb: 4.3.99.99, [4.3-], 4.3.2.0ORCA1, hawq: [1.3-]
       @description test CREATE TABLE with default distribution
       @gpopt 1.510
       @gucs gp_create_table_random_default_distribution=on
       '''

       sql_dir = 'sql_default_distribution_sensitive/'
       ans_dir = 'expected_default_distribution_On/'
       out_dir = 'out/'

class test_create_table_default_distribution_Off(SQLTestCase):
       '''
       testing CREATE TABLE with default distribution
       @author garcic12
       @created 2014-07-01 18:09:00
       @description test CREATE TABLE with default distribution
       @gpopt 1.510
       @gucs gp_create_table_random_default_distribution=off
       @tags CTAS HAWQ ORCA
       @product_version gpdb: 4.3.99.99, [4.3-], 4.3.2.0ORCA1, hawq: [1.3-]
       '''

       sql_dir = 'sql_default_distribution_sensitive/'
       ans_dir = 'expected_default_distribution_Off/'
       out_dir = 'out/'


