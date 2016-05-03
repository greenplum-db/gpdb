@gpcheckcat
Feature: gpcheckcat tests

    Scenario: gpcheckcat should drop leaked schemas
        Given database "leak" is dropped and recreated
        And the user runs the command "psql leak -f 'gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/create_temp_schema_leak.sql'" in the background without sleep
        And waiting "1" seconds
        Then read pid from file "gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/pid_leak" and kill the process
        And the temporary file "gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/pid_leak" is removed
        And waiting "2" seconds
        When the user runs "gpstop -ar"
        Then gpstart should return a return code of 0
        When the user runs "psql leak -f gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/leaked_schema.sql"
        Then psql should return a return code of 0
        And psql should print pg_temp_ to stdout
        And psql should print (1 row) to stdout
        When the user runs "gpcheckcat leak"
        Then gpchekcat should return a return code of 0
        And the user runs "psql leak -f gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/leaked_schema.sql"
        Then psql should return a return code of 0
        And psql should print (0 rows) to stdout
        And verify that the schema "good_schema" exists in "leak"
        And the user runs "dropdb leak"
        And verify that a log was created by gpcheckcat in the user's "gpAdminLogs" directory

    Scenario: gpcheckcat should report unique index violations
        Given database "test_index" is dropped and recreated
        And the user runs "psql test_index -f 'gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/create_unique_index_violation.sql'"
        Then psql should return a return code of 0
        And psql should not print (0 rows) to stdout
        When the user runs "gpcheckcat test_index"
        Then gpcheckcat should not return a return code of 0
        And gpcheckcat should print Table pg_compression has a violated unique index: pg_compression_compname_index to stdout
        And the user runs "dropdb test_index"
        And verify that a log was created by gpcheckcat in the user's "gpAdminLogs" directory

    Scenario Outline: gpcheckcat should discover missing attributes for tables
        Given database "miss_attr" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "miss_attr" with data
        And there is a "heap" partition table "public.heap_part_table" in "miss_attr" with data
        And there is a "ao" table "public.ao_table" in "miss_attr" with data
        And there is a "ao" partition table "public.ao_part_table" in "miss_attr" with data
        And the user runs "psql miss_attr -c "ALTER TABLE heap_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr -c "ALTER TABLE heap_part_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr -c "ALTER TABLE ao_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr -c "ALTER TABLE ao_part_table ALTER COLUMN column1 SET DEFAULT 1;""
        And the user runs "psql miss_attr -c "CREATE RULE notify_me AS ON UPDATE TO heap_table DO ALSO NOTIFY ao_table;""
        And the user runs "psql miss_attr -c "CREATE RULE notify_me AS ON UPDATE TO heap_part_table DO ALSO NOTIFY ao_part_table;""
        And the user runs "psql miss_attr -c "CREATE RULE notify_me AS ON UPDATE TO ao_table DO ALSO NOTIFY heap_table;""
        And the user runs "psql miss_attr -c "CREATE RULE notify_me AS ON UPDATE TO ao_part_table DO ALSO NOTIFY heap_part_table;""
        When the user runs "gpcheckcat miss_attr"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print Missing to stdout
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_table'::regclass::oid;""
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_part_table'::regclass::oid;""
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_table'::regclass::oid;""
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_part_table'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr"
        Then gpcheckcat should print Missing to stdout
        And gpcheckcat should print Table miss_attr.public.heap_table.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.heap_part_table.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.heap_part_table_1_prt_p1_2_prt_1.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_table.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_part_table.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_part_table_1_prt_p1_2_prt_1.-1 to stdout
        Examples:
          | attrname   | tablename     |
          | attrelid   | pg_attribute  |
          | adrelid    | pg_attrdef    |
          | typrelid   | pg_type       |
          | ev_class   | pg_rewrite    |

    Scenario Outline: gpcheckcat should discover missing attributes for indexes
        Given database "miss_attr" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "miss_attr" with data
        And there is a "heap" partition table "public.heap_part_table" in "miss_attr" with data
        And there is a "ao" table "public.ao_table" in "miss_attr" with data
        And there is a "ao" partition table "public.ao_part_table" in "miss_attr" with data
        And the user runs "psql miss_attr -c "CREATE INDEX heap_table_idx on heap_table (column1);""
        And the user runs "psql miss_attr -c "CREATE INDEX heap_part_table_idx on heap_part_table (column1);""
        And the user runs "psql miss_attr -c "CREATE INDEX ao_table_idx on ao_table (column1);""
        And the user runs "psql miss_attr -c "CREATE INDEX ao_part_table_idx on ao_part_table (column1);""
        When the user runs "gpcheckcat miss_attr"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print Missing to stdout
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_table_idx'::regclass::oid;""
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='heap_part_table_idx'::regclass::oid;""
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_table_idx'::regclass::oid;""
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='ao_part_table_idx'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr"
        Then gpcheckcat should print Missing to stdout
        And gpcheckcat should print Table miss_attr.public.heap_table_idx.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.heap_part_table_idx.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_table_idx.-1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_part_table_idx.-1 to stdout
        Examples:
          | attrname   | tablename    |
          | indexrelid | pg_index     |

    Scenario Outline: gpcheckcat should discover missing attributes for external tables
        Given database "miss_attr" is dropped and recreated
        And the user runs "echo > /tmp/backup_gpfdist_dummy"
        And the user runs "gpfdist -p 8098 -d /tmp &"
        And there is a partition table "part_external" has external partitions of gpfdist with file "backup_gpfdist_dummy" on port "8098" in "miss_attr" with data
        Then data for partition table "part_external" with partition level "0" is distributed across all segments on "miss_attr"
        When the user runs "gpcheckcat miss_attr"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print Missing to stdout
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='part_external_1_prt_p_2'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr"
        Then gpcheckcat should print Missing to stdout
        And gpcheckcat should print Table miss_attr.public.part_external_1_prt_p_2.-1 to stdout
        Examples:
          | attrname   | tablename     |
          | reloid     | pg_exttable   |
          | fmterrtbl  | pg_exttable   |
          | conrelid   | pg_constraint |

    Scenario: gpcheckcat should print out tables with missing and extraneous attributes in a readable format
        Given database "miss_attr" is dropped and recreated
        And there is a "heap" table "public.heap_table" in "miss_attr" with data
        And there is a "ao" table "public.ao_table" in "miss_attr" with data
        When the user runs "gpcheckcat miss_attr"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print Missing to stdout
        And an attribute of table "heap_table" in database "miss_attr" is deleted on segment with content id "0"
        And psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr"
        Then gpcheckcat should print Missing to stdout
        And gpcheckcat should print Table miss_attr.public.heap_table.0 to stdout
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM pg_attribute where attrelid='heap_table'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr"
        Then gpcheckcat should print Extra to stdout
        And gpcheckcat should print Table miss_attr.public.heap_table.1 to stdout

    Scenario: gpcheckcat should find owner error and produce timestamped repair scripts from -A (all databases) option
        Given database "db1" is dropped and recreated
        And database "db2" is dropped and recreated
        And the path "gpcheckcat.repair.*" is removed from current working directory
        And there is a "heap" table "gpadmin_tbl" in "db1" with data
        And there is a "heap" table "gpadmin_tbl" in "db2" with data
        And the user runs "psql db1 -f gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/create_user_wolf.sql"
        Then psql should return a return code of 0
        Given the user runs sql "alter table gpadmin_tbl OWNER TO wolf" in "db1" on first primary segment
        When the user runs "gpcheckcat -A"
        Then gpcheckcat should return a return code of 3
        Then gpcheckcat should print reported here: owner to stdout
        Then the path "gpcheckcat.repair.*" is found in cwd "1" times
        When the user runs "gpcheckcat -A"
        Then gpcheckcat should return a return code of 3
        Then gpcheckcat should print reported here: owner to stdout
        Then the path "gpcheckcat.repair.*" is found in cwd "2" times
        And the user runs "dropdb db1"
        And the user runs "dropdb db2"
        And the path "gpcheckcat.repair.*" is removed from current working directory

    @persistent
    Scenario: gpcheckcat should find persistence errors
        Given database "db1" is dropped and recreated
        And there is a "heap" table "myheaptable1" in "db1" with data
        And there is a "heap" table "myheaptable2" in "db1" with data
        And there is a "heap" table "myheaptable3" in "db1" with data
        And there is a "heap" table "myheaptable4" in "db1" with data
        And there is a "heap" table "myheaptable5" in "db1" with data
        And the user runs "psql db1 -c "select gp_delete_persistent_relation_node_entry(ctid) from (select ctid from gp_persistent_relation_node where relfilenode_oid=(select relfilenode from pg_class where relname = 'myheaptable2')) as unwanted;""
        And the user runs "psql db1 -c "select gp_delete_persistent_relation_node_entry(ctid) from (select ctid from gp_persistent_relation_node where relfilenode_oid=(select relfilenode from pg_class where relname = 'myheaptable3')) as unwanted;""
        When the user runs "gpcheckcat -R persistent db1"
        Then gpcheckcat should print Failed test\(s\) that are not reported here: persistent to stdout

    Scenario: gpcheckcat should raise exception if the database already contains a function "readindex(oid, bool)" in the schema gp_toolkit
        Given database "test_readindex_exists" is dropped and recreated
        When the user runs "psql test_readindex_exists -f 'gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/create_readindex_function.sql'"
        And the user runs "gpcheckcat test_readindex_exists"
        Then gpcheckcat should return a return code of 3
        And gpcheckcat should print Function readindex\(oid,bool\) already exists in schema gp_toolkit to stdout
        And the user runs "dropdb test_readindex_exists"
        And verify that a log was created by gpcheckcat in the user's "gpAdminLogs" directory

    Scenario: gpcheckcat should raise exception if the database already contains a type "readindex_type" in the schema gp_toolkit
        Given database "test_readindex_type_exists" is dropped and recreated
        When the user runs "psql test_readindex_type_exists -f 'gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/create_readindex_type.sql'"
        And the user runs "gpcheckcat test_readindex_type_exists"
        Then gpcheckcat should return a return code of 3
        And gpcheckcat should print Type readindex_type already exists in schema gp_toolkit to stdout
        And the user runs "dropdb test_readindex_type_exists"
        And verify that a log was created by gpcheckcat in the user's "gpAdminLogs" directory

    Scenario: gpcheckcat should report duplicate index values
        Given database "test_duplicate_index" is dropped and recreated
        When the user runs "gpcheckcat test_duplicate_index"
        Then gpcheckcat should return a return code of 0
        Given duplicate index values for index "pg_compression_compname_index" in database "test_duplicate_index"
        When the user runs "gpstop -ar"
        Then gpstart should return a return code of 0
        When the user runs "gpcheckcat test_duplicate_index"
        Then gpcheckcat should not return a return code of 0
        And gpcheckcat should print Table pg_compression has a violated unique index: pg_compression_compname_index to stdout
        Then gpcheckcat should not return a return code of 0
        And verify that the function "readindex" does not exist in schema "gp_toolkit" in database "test_duplicate_index"
        Then verify that the type "readindex_type" does not exist in schema "gp_toolkit" in database "test_duplicate_index"
        And the user runs "dropdb test_duplicate_index"
        And verify that a log was created by gpcheckcat in the user's "gpAdminLogs" directory
