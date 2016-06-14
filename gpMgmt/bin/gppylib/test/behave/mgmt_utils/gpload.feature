@gpload
Feature: gpload tests

    Scenario: gpload without -l option should create log file in default location 
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        And there is a "heap" table "t1" with compression "None" in "testdb" with data
        And the user truncates "public.t1" tables in "testdb"
        And the gpAdminLogs directory has been backed up
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_ctl_fmt.yml" 
        Then gpload should return a return code of 0
        And the "directory" path "~/gpAdminLogs" should " " exist
        And the directory "~/gpAdminLogs" is removed

    Scenario: multiple gpload instances on the same target table using "reuse_tables" won't truncate each other. 
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When table "tbl" is deleted in "testdb"
        When the user runs the query "create table tbl (id int, value int)" on "testdb"
        When table "staging_gpload_reusable_3d34bd915c21789de84df0d76251fe1f" is deleted in "testdb"
        When the user runs the query "create table staging_gpload_reusable_3d34bd915c21789de84df0d76251fe1f(id int, value int)" on "testdb"
        When the user runs command "gppylib/test/behave/mgmt_utils/steps/data/gpload_cmd.sh" 
        When execute sql "select count(*)::text from tbl where value=888" in db "testdb" and store result in the context
        Then validate that "100000" is in the stored rows
        When execute sql "select count(*)::text from tbl where value=999" in db "testdb" and store result in the context
        Then validate that "100000" is in the stored rows

    Scenario: gpload creates staging table with the right columns 
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When table "tbl1" is deleted in "testdb"
        When the user runs the query "create table tbl1 (id int, msg text, updated_ts timestamp) distributed by (id)" on "testdb"
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_staging_with_mapping.yml"
        When execute sql "select * from tbl1" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | id   |  msg    |   updated_ts         |
            | 1    | aaaaa   |  2013-08-11 19:30:54 | 
        When execute sql "select column_name from INFORMATION_SCHEMA.COLUMNS where table_name = 'staging_gpload_reusable_a155bc557bb7db43ea87623246e168e4'" in db "testdb" and store result in the context 
        Then validate that following rows are in the stored rows
            | column_name |
            | id          |
            | msg         |
            | updated_ts  |
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_staging_without_mapping.yml"
        When execute sql "select * from tbl1" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | id   |  msg    |   updated_ts         |
            | 2    | bbbbb   |  None                | 
            | 1    | aaaaa   |  2013-08-11 19:30:54 |
        When execute sql "select column_name from INFORMATION_SCHEMA.COLUMNS where table_name = 'staging_gpload_reusable_a8910d328968eff2f55a5b751746c922'" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | column_name |
            | id          |
            | msg         |

    #For scenario like this: method "update", column with "not null" constraint, staging table should not inherite the constraint (the staging table can't be created using "create ... like ...").
    Scenario: gpload creates staging table without constraints   
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When table "tbl2" is deleted in "testdb"
        When the user runs the query "create table tbl2 (id int, msg text not null, updated_ts timestamp) distributed by (id)" on "testdb"
        When the user runs the query "insert into tbl2 values(1, 'aaaaa', now())" on "testdb"
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_staging_not_null.yml"
        When execute sql "select * from tbl2" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | id   |  msg    |   updated_ts         |
            | 1    | aaaaa   |  2013-08-11 19:30:54 |  

    Scenario: gpload creates staging table with control character delimiter
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When table "tbl3" is deleted in "testdb"
        When the user runs the query "create table tbl3 (col1 int, col2 int)" on "testdb"
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_ctrl_delimiter.yml"
        When execute sql "select * from tbl3 order by col1" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | col1   |  col2    |
            | 111    |  11      |
            | 222    |  22      |

    Scenario: gpload reuse external table when having the same source and different target tables 
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When table "tbl1" is deleted in "testdb"
        When table "tbl2" is deleted in "testdb"
        When the user runs the query "create table tbl1 (id int, msg text) distributed by (id)" on "testdb"
        When the user runs the query "create table tbl2 (id int, msg text, other text) distributed by (id)" on "testdb"
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_reuse_ext_1.yml"
        When execute sql "select * from tbl1" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | id   |  msg       |
            | 123  | reuse_test |
        When the user runs "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_reuse_ext_2.yml"
        When execute sql "select * from tbl2" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | id   |  msg       |   other  |
            | 123  | reuse_test |   None   |
        When execute sql "SELECT count(*) FROM pg_class WHERE relname like 'ext_gpload_reusable%'" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | count   |
            | 1       |

    @gpload_target_not_exist
    Scenario: gpload output error message when target table not exists 
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs command "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_target_not_exist.yml"
        Then the client program should print table "tablenot11exists" not found in any database schema to stdout

    @gpload_ext_schema
    Scenario: gpload create external table with specific schema if external:schema is set in yaml file
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs the query "create table tbl1 (id int, msg text) distributed by (id)" on "testdb"
        When the user runs the query "create schema gpload_ext_schema" on "testdb"
        When the user runs command "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_ext_schema_specified.yml"
        When execute sql "select count(*) from pg_tables where schemaname='gpload_ext_schema'" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | count   |
            | 1       |


    Scenario: gpload create external table with the same schema as output:table if external:schema is '%' and output:table contain a schema
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs the query "create schema gpload_ext_schema" on "testdb"
        When the user runs the query "create table gpload_ext_schema.tbl1 (id int, msg text) distributed by (id)" on "testdb"
        When the user runs command "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_ext_schema_percent_sign.yml"
        When execute sql "select count(*) from pg_tables where schemaname='gpload_ext_schema'" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | count   |
            | 2       |

    Scenario: gpload create external table with the current schema in search_path if external:schema is '%' and output:table dosen't contain a schema
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs the query "create table tbl1 (id int, msg text) distributed by (id)" on "testdb"
        When the user runs command "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_ext_schema_percent_sign_no_schema.yml"
        When execute sql "select count(*) from pg_tables where schemaname='public' and tablename like 'ext_gpload_reusable%'" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | count   |
            | 1       |


    Scenario: gpload create external table with the current schema in the search_path  if external:schema is not set in yaml file
        Given the database is running
        And the database "testdb" does not exist
        And database "testdb" exists
        When the user runs the query "create schema gpload_ext_schema" on "testdb"
        When the user runs the query "create table tbl1 (id int, msg text) distributed by (id)" on "testdb"
        When the user runs command "gpload -f gppylib/test/behave/mgmt_utils/steps/data/gpload_ext_schema_unspecified.yml"
        When execute sql "select count(*) from pg_tables where schemaname='gpload_ext_schema'" in db "testdb" and store result in the context
        Then validate that following rows are in the stored rows
            | count   |
            | 1       |
