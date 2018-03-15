@gpcheckcat
Feature: gpcheckcat tests

    @logging
    Scenario: gpcheckcat should log into gpAdminLogs
        When the user runs "gpcheckcat -l"
        Then verify that the utility gpcheckcat ever does logging into the user's "gpAdminLogs" directory

    @all
    Scenario: run all the checks in gpcheckcat
        Given database "all_good" is dropped and recreated
        Then the user runs "gpcheckcat -A"
        Then gpcheckcat should return a return code of 0
        And the user runs "dropdb all_good"

    @leak
    Scenario: gpcheckcat should drop leaked schemas
        Given database "leak_db" is dropped and recreated
        And the user runs the command "psql leak_db -f 'test/behave/mgmt_utils/steps/data/gpcheckcat/create_temp_schema_leak.sql'" in the background without sleep
        And waiting "1" seconds
        Then read pid from file "test/behave/mgmt_utils/steps/data/gpcheckcat/pid_leak" and kill the process
        And the temporary file "test/behave/mgmt_utils/steps/data/gpcheckcat/pid_leak" is removed
        And waiting "2" seconds
        When the user runs "gpstop -ar"
        Then gpstart should return a return code of 0
        When the user runs "psql leak_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/leaked_schema.sql"
        Then psql should return a return code of 0
        And psql should print "pg_temp_" to stdout
        And psql should print "(1 row)" to stdout
        When the user runs "gpcheckcat leak_db"
        Then gpchekcat should return a return code of 0
        Then gpcheckcat should print "Found and dropped 2 unbound temporary schemas" to stdout
        And the user runs "psql leak_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/leaked_schema.sql"
        Then psql should return a return code of 0
        And psql should print "(0 rows)" to stdout
        And verify that the schema "good_schema" exists in "leak_db"
        And the user runs "dropdb leak_db"

    @unique_index
    Scenario: gpcheckcat should report unique index violations
        Given database "unique_index_db" is dropped and recreated
        And the user runs "psql unique_index_db -f 'test/behave/mgmt_utils/steps/data/gpcheckcat/create_unique_index_violation.sql'"
        Then psql should return a return code of 0
        And psql should not print "(0 rows)" to stdout
        When the user runs "gpcheckcat unique_index_db"
        Then gpcheckcat should return a return code of 3
        And gpcheckcat should print "Table pg_compression has a violated unique index: pg_compression_compname_index" to stdout
        And the user runs "dropdb unique_index_db"

    @miss_attr_table
    Scenario Outline: gpcheckcat should discover missing attributes for tables
        Given database "miss_attr_db1" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "miss_attr_db1" with data
        And there is a "heap" partition table "public.heap_part_table" in "miss_attr_db1" with data
        And there is a "ao" table "public.ao_table" in "miss_attr_db1" with data
        And there is a "ao" partition table "public.ao_part_table" in "miss_attr_db1" with data
        And the user runs "psql miss_attr_db1 -c "ALTER TABLE heap_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr_db1 -c "ALTER TABLE heap_part_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr_db1 -c "ALTER TABLE ao_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr_db1 -c "ALTER TABLE ao_part_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr_db1 -c "CREATE RULE notify_me AS ON UPDATE TO heap_table DO ALSO NOTIFY ao_table;""
        And the user runs "psql miss_attr_db1 -c "CREATE RULE notify_me AS ON UPDATE TO heap_part_table DO ALSO NOTIFY ao_part_table;""
        And the user runs "psql miss_attr_db1 -c "CREATE RULE notify_me AS ON UPDATE TO ao_table DO ALSO NOTIFY heap_table;""
        And the user runs "psql miss_attr_db1 -c "CREATE RULE notify_me AS ON UPDATE TO ao_part_table DO ALSO NOTIFY heap_part_table;""
        When the user runs "gpcheckcat miss_attr_db1"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print "Missing" to stdout
        And the user runs "psql miss_attr_db1 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_table'::regclass::oid;""
        And the user runs "psql miss_attr_db1 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_part_table'::regclass::oid;""
        And the user runs "psql miss_attr_db1 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_table'::regclass::oid;""
        And the user runs "psql miss_attr_db1 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_part_table'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr_db1"
        Then gpcheckcat should print "Missing" to stdout
        And gpcheckcat should print "Table miss_attr_db1.public.heap_table.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db1.public.heap_part_table.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db1.public.heap_part_table_1_prt_p1_2_prt_1.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db1.public.ao_table.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db1.public.ao_part_table.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db1.public.ao_part_table_1_prt_p1_2_prt_1.-1" to stdout
        And gpcheckcat should print "on content -1" to stdout
        Examples:
          | attrname   | tablename     |
          | attrelid   | pg_attribute  |
          | adrelid    | pg_attrdef    |
          | typrelid   | pg_type       |
          | ev_class   | pg_rewrite    |

    @miss_attr_db_index
    Scenario Outline: gpcheckcat should discover missing attributes for indexes
        Given database "miss_attr_db2" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "miss_attr_db2" with data
        And there is a "heap" partition table "public.heap_part_table" in "miss_attr_db2" with data
        And there is a "ao" table "public.ao_table" in "miss_attr_db2" with data
        And there is a "ao" partition table "public.ao_part_table" in "miss_attr_db2" with data
        And the user runs "psql miss_attr_db2 -c "CREATE INDEX heap_table_idx on heap_table (column1);""
        And the user runs "psql miss_attr_db2 -c "CREATE INDEX heap_part_table_idx on heap_part_table (column1);""
        And the user runs "psql miss_attr_db2 -c "CREATE INDEX ao_table_idx on ao_table (column1);""
        And the user runs "psql miss_attr_db2 -c "CREATE INDEX ao_part_table_idx on ao_part_table (column1);""
        When the user runs "gpcheckcat miss_attr_db2"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print "Missing" to stdout
        And the user runs "psql miss_attr_db2 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_table_idx'::regclass::oid;""
        And the user runs "psql miss_attr_db2 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_part_table_idx'::regclass::oid;""
        And the user runs "psql miss_attr_db2 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_table_idx'::regclass::oid;""
        And the user runs "psql miss_attr_db2 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_part_table_idx'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr_db2"
        Then gpcheckcat should print "Missing" to stdout
        And gpcheckcat should print "Table miss_attr_db2.public.heap_table_idx.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db2.public.heap_part_table_idx.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db2.public.ao_table_idx.-1" to stdout
        And gpcheckcat should print "Table miss_attr_db2.public.ao_part_table_idx.-1" to stdout
        Examples:
          | attrname   | tablename    |
          | indexrelid | pg_index     |

    @miss_attr_table
    Scenario Outline: gpcheckcat should discover missing attributes for external tables
        Given database "miss_attr_db3" is dropped and recreated
        And the user runs "echo > /tmp/backup_gpfdist_dummy"
        And the user runs "gpfdist -p 8098 -d /tmp &"
        And there is a partition table "part_external" has external partitions of gpfdist with file "backup_gpfdist_dummy" on port "8098" in "miss_attr_db3" with data
        Then data for partition table "part_external" with partition level "0" is distributed across all segments on "miss_attr_db3"
        When the user runs "gpcheckcat miss_attr_db3"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print "Missing" to stdout
        And the user runs "psql miss_attr_db3 -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='part_external_1_prt_p_2'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr_db3"
        Then gpcheckcat should print "Missing" to stdout
        And gpcheckcat should print "Table miss_attr_db3.public.part_external_1_prt_p_2.-1" to stdout
        Examples:
          | attrname   | tablename     |
          | reloid     | pg_exttable   |
          | fmterrtbl  | pg_exttable   |
          | conrelid   | pg_constraint |

    @miss_attr_table
    Scenario: gpcheckcat should print out tables with missing and extraneous attributes in a readable format
        Given database "miss_attr_db4" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "miss_attr_db4" with data
        And there is a "ao" table "public.ao_table" in "miss_attr_db4" with data
        When the user runs "gpcheckcat miss_attr_db4"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print "Missing" to stdout
        And an attribute of table "heap_table" in database "miss_attr_db4" is deleted on segment with content id "0"
        And psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr_db4"
        Then gpcheckcat should print "Missing" to stdout
        And gpcheckcat should print "Table miss_attr_db4.public.heap_table.0" to stdout
        And the user runs "psql miss_attr_db4 -c "SET allow_system_table_mods='dml'; DELETE FROM pg_attribute where attrelid='heap_table'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr_db4"
        Then gpcheckcat should print "Extra" to stdout
        And gpcheckcat should print "Table miss_attr_db4.public.heap_table.1" to stdout

    @owner
    Scenario: gpcheckcat should report and repair owner errors and produce timestamped repair scripts
        Given database "owner_db1" is dropped and recreated
        And database "owner_db2" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And there is a "heap" table "gpadmin_tbl" in "owner_db1" with data
        And there is a "heap" table "gpadmin_tbl" in "owner_db2" with data
        And the user runs "psql owner_db1 -f test/behave/mgmt_utils/steps/data/gpcheckcat/create_user_wolf.sql"
        Then psql should return a return code of 0
        Given the user runs sql "alter table gpadmin_tbl OWNER TO wolf" in "owner_db1" on first primary segment
        When the user runs "gpcheckcat -R owner owner_db1"
        Then gpcheckcat should return a return code of 3
        Then the path "gpcheckcat.repair.*" is found in cwd "1" times
        Then gpcheckcat should print "reported here: owner" to stdout
        And waiting "1" seconds
        When the user runs "gpcheckcat -R owner owner_db1"
        Then gpcheckcat should return a return code of 3
        Then the path "gpcheckcat.repair.*" is found in cwd "2" times
        Then gpcheckcat should print "reported here: owner" to stdout
        Then run all the repair scripts in the dir "gpcheckcat.repair.*"
        And the path "gpcheckcat.repair.*" is removed from current working directory
        When the user runs "gpcheckcat -R owner owner_db1"
        Then gpcheckcat should return a return code of 0
        Then the path "gpcheckcat.repair.*" is found in cwd "0" times
        When the user runs "gpcheckcat -R owner owner_db2"
        Then gpcheckcat should return a return code of 0
        Then the path "gpcheckcat.repair.*" is found in cwd "0" times
        And the user runs "dropdb owner_db1"
        And the user runs "dropdb owner_db2"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @constraint
    Scenario: gpcheckcat should report and repair invalid constraints
        Given database "constraint_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user runs "psql constraint_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/create_invalid_constraint.sql"
        Then psql should return a return code of 0
        When the user runs "gpcheckcat -R distribution_policy constraint_db"
        Then gpcheckcat should return a return code of 1
        Then validate and run gpcheckcat repair
        When the user runs "gpcheckcat -R constraint constraint_db"
        Then the path "gpcheckcat.repair.*" is found in cwd "0" times
        And the user runs "dropdb constraint_db"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @policy
    Scenario: gpcheckcat should report and repair invalid policy issues
        Given database "policy_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user runs "psql policy_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/create_inconsistent_policy.sql"
        Then psql should return a return code of 0
        When the user runs "gpcheckcat -R part_integrity policy_db"
        Then gpcheckcat should return a return code of 1
        Then validate and run gpcheckcat repair
        And the user runs "dropdb policy_db"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @foreignkey_extra
    Scenario: gpcheckcat foreign key check should report missing catalog entries. Also test missing_extraneous for the same case.
        Given database "fkey_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And there is a "heap" table "gpadmin_tbl" in "fkey_db" with data
        When the entry for the table "gpadmin_tbl" is removed from "pg_catalog.pg_class" with key "oid" in the database "fkey_db"
        Then the user runs "gpcheckcat -E -R missing_extraneous fkey_db"
        And gpcheckcat should print "Name of test which found this issue: missing_extraneous_pg_class" to stdout
        Then gpcheckcat should return a return code of 1
        Then validate and run gpcheckcat repair
        Then the user runs "gpcheckcat -E -R foreign_key fkey_db"
        Then gpcheckcat should print "No pg_class {.*} entry for pg_attribute {.*}" to stdout
        Then gpcheckcat should print "No pg_class {.*} entry for pg_type {.*}" to stdout
        Then gpcheckcat should print "No pg_class {.*} entry for gp_distribution_policy {.*}" to stdout
        Then gpcheckcat should return a return code of 3
        Then the user runs "gpcheckcat -E -R missing_extraneous fkey_db"
        Then gpcheckcat should return a return code of 0
        Then the path "gpcheckcat.repair.*" is found in cwd "0" times
        And the user runs "dropdb fkey_db"

    @foreignkey_full_segment
    Scenario Outline: gpcheckcat foreign key check should report missing catalog entries for segments. Also test missing_extraneous for the same case.
        Given database "fkey_ta" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user creates an index for table "index_table" in database "fkey_ta"
        And there is a "ao" table "ao_table" in "fkey_ta" with data
        When the entry for the table "<table_name>" is removed from "pg_catalog.<catalog_name>" with key "<catalog_oid_key>" in the database "fkey_ta" on the first primary segment
        Then the user runs "gpcheckcat -E -R foreign_key fkey_ta"
        Then gpcheckcat should print "No <catalog_name> {.*} entry for pg_class {.*}" to stdout
        Then gpcheckcat should return a return code of 3
        And the user runs "dropdb fkey_ta"
        Examples:
          | catalog_name                | catalog_oid_key | table_name |
          | pg_attribute                | attrelid        | index_table |
          | pg_index                    | indrelid        | index_table |
          | pg_appendonly               | relid           | ao_table   |

    @foreignkey_full_master
    Scenario Outline: gpcheckcat foreign key check should report missing catalog entries. Also test missing_extraneous for the same case.
        Given database "fkey_ta" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user creates an index for table "index_table" in database "fkey_ta"
        And there is a "ao" table "ao_table" in "fkey_ta" with data
        When the entry for the table "<table_name>" is removed from "pg_catalog.<catalog_name>" with key "<catalog_oid_key>" in the database "fkey_ta"
        Then the user runs "gpcheckcat -E -R foreign_key fkey_ta"
        Then gpcheckcat should print "No <catalog_name> {.*} entry for pg_class {.*}" to stdout
        Then gpcheckcat should return a return code of 3
        And the user runs "dropdb fkey_ta"
        Examples:
          | catalog_name                | catalog_oid_key | table_name |
          | pg_attribute                | attrelid        | index_table |
          | pg_index                    | indrelid        | index_table |
          | pg_appendonly               | relid           | ao_table   |

    @foreignkey_type
    Scenario: gpcheckcat foreign key check should report missing catalog entries. Also test missing_extraneous for the same case.
        Given database "fkey_ta" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And there is a "heap" table "gpadmin_tbl" in "fkey_ta" with data
        When the entry for the table "gpadmin_tbl" is removed from "pg_catalog.pg_type" with key "typrelid" in the database "fkey_ta"
        Then the user runs "gpcheckcat -E -R foreign_key fkey_ta"
        Then gpcheckcat should print "No pg_type {.*} entry for pg_class {.*}" to stdout
        Then gpcheckcat should return a return code of 3
        And the user runs "dropdb fkey_ta"

    @tt
    @extra
    Scenario: gpcheckcat should report and repair extra entries with non-oid primary keys
        Given database "extra_pk_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user runs "psql extra_pk_db -c 'CREATE SCHEMA my_pk_schema' "
        And the user runs "psql extra_pk_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/add_operator.sql "
        Then psql should return a return code of 0
        And the user runs "psql extra_pk_db -c "set allow_system_table_mods=DML;DELETE FROM pg_catalog.pg_operator where oprname='!#'" "
        Then psql should return a return code of 0
        When the user runs "gpcheckcat -R missing_extraneous extra_pk_db"
        Then gpcheckcat should return a return code of 3
        And the path "gpcheckcat.repair.*" is found in cwd "0" times
        When the user runs "gpcheckcat -R missing_extraneous -E extra_pk_db"
        Then gpcheckcat should return a return code of 1
        And validate and run gpcheckcat repair
        When the user runs "gpcheckcat -R missing_extraneous -E extra_pk_db"
        Then gpcheckcat should return a return code of 0
        And the user runs "dropdb extra_pk_db"
        And the path "gpcheckcat.repair.*" is removed from current working directory


    @extra
    Scenario: gpcheckcat should report and repair extra entries in master as well as all the segments
        Given database "extra_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user runs "psql extra_db -c "CREATE TABLE foo(i int)""
        Then The user runs sql "set allow_system_table_mods=DML;delete from pg_class where relname='foo'" in "extra_db" on first primary segment
        And the user runs "psql extra_db -c "drop table if exists foo""
        Then the user runs "gpcheckcat -R missing_extraneous extra_db"
        Then gpcheckcat should return a return code of 3
        Then the path "gpcheckcat.repair.*" is found in cwd "0" times
        Then the user runs "gpcheckcat -R missing_extraneous -E extra_db"
        Then gpcheckcat should return a return code of 1
        Then validate and run gpcheckcat repair
        When the user runs "gpcheckcat -R missing_extraneous -E extra_db"
        Then gpcheckcat should return a return code of 0
        Then the path "gpcheckcat.repair.*" is found in cwd "0" times
        And the user runs "dropdb extra_db"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @foreignkey_gp_fastsequence
    Scenario: gpcheckcat should report inconsistency between gp_fastsequence and pg_class
        Given database "fkey2_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user runs "psql fkey2_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/create_aoco_table.sql"
        And the user runs sql file "test/behave/mgmt_utils/steps/data/gpcheckcat/create_inconsistent_gpfastsequence.sql" in "fkey2_db" on all the segments
        Then the user runs "gpcheckcat fkey2_db"
        Then gpcheckcat should return a return code of 3
        Then gpcheckcat should print "No pg_class {.*} entry for gp_fastsequence {.*}" to stdout
        Then validate and run gpcheckcat repair
        Then the user runs "gpcheckcat -R foreign_key fkey2_db"
        Then gpcheckcat should not print "No pg_class {.*} entry for gp_fastsequence {.*}" to stdout
        Then gpcheckcat should return a return code of 3
        And the user runs "dropdb fkey2_db"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @extra_gr
    Scenario: gpcheckcat should generate repair scripts when -g, -R, and -E options are provided
        Given database "extra_gr_db" is dropped and recreated
        And the path "repair_dir" is removed from current working directory
        And the user runs "psql extra_gr_db -c "CREATE TABLE foo(i int)""
        Then The user runs sql "set allow_system_table_mods=DML;delete from pg_class where relname='foo'" in "extra_gr_db" on first primary segment
        And the user runs "psql extra_gr_db -c "drop table if exists foo""
        Then the user runs "gpcheckcat -R missing_extraneous -E -g repair_dir extra_gr_db"
        Then gpcheckcat should return a return code of 1
        Then gpcheckcat should print "repair script\(s\) generated in dir repair_dir" to stdout
        Then the path "repair_dir" is found in cwd "1" times
        Then run all the repair scripts in the dir "repair_dir"
        And the path "repair_dir" is removed from current working directory
        When the user runs "gpcheckcat -R missing_extraneous -E -g repair_dir extra_gr_db"
        Then gpcheckcat should return a return code of 0
        Then the path "repair_dir" is found in cwd "0" times
        And the user runs "dropdb extra_gr_db"
        And the path "repair_dir" is removed from current working directory

    @constraint_g
    Scenario: gpcheckcat should generate repair scripts when only -g option is provided
        Given database "constraint_g_db" is dropped and recreated
        And the user runs "psql constraint_g_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/create_invalid_constraint.sql"
        Then psql should return a return code of 0
        When the user runs "gpcheckcat -g repair_dir constraint_g_db"
        Then gpcheckcat should return a return code of 1
        Then gpcheckcat should print "repair script\(s\) generated in dir repair_dir" to stdout
        Then the path "repair_dir" is found in cwd "1" times
        Then run all the repair scripts in the dir "repair_dir"
        And the path "repair_dir" is removed from current working directory
        When the user runs "gpcheckcat -g repair_dir constraint_g_db"
        Then gpcheckcat should return a return code of 0
        Then the path "repair_dir" is found in cwd "0" times
        And the user runs "dropdb constraint_g_db"
        And the path "repair_dir" is removed from current working directory

    @timestamp
    Scenario: gpcheckcat should use the same timestamp for creating repair dir and scripts
        Given database "timestamp_db" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And the user runs "psql timestamp_db -f test/behave/mgmt_utils/steps/data/gpcheckcat/create_aoco_table.sql"
        And the user runs sql file "test/behave/mgmt_utils/steps/data/gpcheckcat/create_inconsistent_gpfastsequence.sql" in "timestamp_db" on all the segments
        And the user runs "psql timestamp_db -c "CREATE TABLE foo(i int)""
        Then The user runs sql "set allow_system_table_mods=DML;delete from pg_class where relname='foo'" in "timestamp_db" on first primary segment
        And the user runs "psql timestamp_db -c "drop table if exists foo""
        Then the user runs "gpcheckcat timestamp_db"
        Then gpcheckcat should return a return code of 3
        Then the timestamps in the repair dir are consistent
        And the user runs "dropdb timestamp_db"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @catalog_dependency
    Scenario: gpcheckcat missing_extraneous and dependency tests detects pg_depend issues
        Given database "gpcheckcat_dependency" is dropped and recreated
        And there is a "heap" table "heap_table1" in "gpcheckcat_dependency" with data
        And there is a "heap" table "heap_table2" in "gpcheckcat_dependency" with data
        And there is a "heap" table "heap_table3" in "gpcheckcat_dependency" with data
        And the entry for the table "heap_table1" is removed from "pg_catalog.pg_depend" with key "objid" in the database "gpcheckcat_dependency" on the first primary segment
        And the entry for the table "heap_table1" is removed from "pg_catalog.pg_depend" with key "refobjid" in the database "gpcheckcat_dependency" on the first primary segment
        And the entry for the table "heap_table2" is removed from "pg_catalog.pg_type" with key "typrelid" in the database "gpcheckcat_dependency" on the first primary segment
        And the entry for the table "heap_table3" is removed from "pg_catalog.pg_depend" with key "refobjid" in the database "gpcheckcat_dependency" on the first primary segment
        And table "heap_table3" is dropped in "gpcheckcat_dependency"
        When the user runs "gpcheckcat gpcheckcat_dependency"
        Then gpcheckcat should return a return code of 3
        Then gpcheckcat should print "Name of test which found this issue: missing_extraneous_pg_type" to stdout
        Then gpcheckcat should print "Extra type metadata of {.*} on content 0" to stdout
        Then gpcheckcat should print "Name of test which found this issue: missing_extraneous_pg_depend" to stdout
        Then gpcheckcat should print "Extra depend metadata of {.*} on content 0" to stdout
        Then gpcheckcat should print "Missing depend metadata of {.*} on content 0" to stdout
        Then gpcheckcat should print "Name of test which found this issue: dependency_pg_class" to stdout
        Then gpcheckcat should print "Table pg_class has a dependency issue on oid .* at content 0" to stdout
        Then gpcheckcat should print "Name of test which found this issue: dependency_pg_type" to stdout
        Then gpcheckcat should print "Table pg_type has a dependency issue on oid .* at content 0" to stdout
        And the user runs "dropdb gpcheckcat_dependency"
