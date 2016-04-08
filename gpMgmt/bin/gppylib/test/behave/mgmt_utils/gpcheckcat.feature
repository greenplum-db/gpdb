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
        And the user runs "psql leak -f gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/leaked_schema.sql"
        Then psql should return a return code of 0
        And psql should print (0 rows) to stdout
        And verify that the schema "good_schema" exists in "leak"
        And the user runs "dropdb leak"

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
        And gpcheckcat should print Table miss_attr.public.heap_table on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.heap_part_table on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.heap_part_table_1_prt_p1_2_prt_1 on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_table on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_part_table on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_part_table_1_prt_p1_2_prt_1 on segment\(s\) -1 to stdout
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
        And gpcheckcat should print Table miss_attr.public.heap_table_idx on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.heap_part_table_idx on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_table_idx on segment\(s\) -1 to stdout
        And gpcheckcat should print Table miss_attr.public.ao_part_table_idx on segment\(s\) -1 to stdout
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
        And gpcheckcat should print Table miss_attr.public.part_external_1_prt_p_2 on segment\(s\) -1 to stdout
        Examples:
          | attrname   | tablename     |
          | reloid     | pg_exttable   |
          | fmterrtbl  | pg_exttable   |
          | conrelid   | pg_constraint |
