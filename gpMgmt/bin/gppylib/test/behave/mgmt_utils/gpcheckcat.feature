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

    @foo
    Scenario Outline: gpcheckcat should discover attributes missing from pg_class
        Given database "miss_attr" is dropped and recreated
        And there is a "heap" table "public.foo" in "miss_attr" with data
        And the user runs "psql miss_attr -c "ALTER TABLE foo ALTER COLUMN column1 SET DEFAULT 1;""
        When the user runs "gpcheckcat miss_attr"
        And gpcheckcat should return a return code of 0
        Then gpcheckcat should not print Missing to stdout
        And the user runs "psql miss_attr -c "SET allow_system_table_mods='dml'; DELETE FROM <tablename> where <attrname>='foo'::regclass::oid;""
        Then psql should return a return code of 0
        When the user runs "gpcheckcat miss_attr"
        Then gpcheckcat should print Missing to stdout
        Examples:
          | attrname | tablename    |
          | attrelid | pg_attribute |
          | adrelid  | pg_attrdef   |
          | typrelid | pg_type      |
