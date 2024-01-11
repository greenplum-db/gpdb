@minirepro
Feature: Dump minimum database objects that is related to the query

    @minirepro_UI
    Scenario: Invalid arguments entered
      When the user runs "minirepro -w"
      Then minirepro should print "error: no such option" error message
      When the user runs "minirepro minireprodb -w"
      Then minirepro should print "error: no such option" error message

    @minirepro_UI
    Scenario: Missing required parameters
      When the user runs "minirepro"
      Then minirepro should print "error: No database specified" error message
      When the user runs "minirepro minireprodb -q"
      Then minirepro should print "error: -q option requires an argument" error message
      When the user runs "minirepro minireprodb -q ~/in.sql"
      Then minirepro should print "error: No output file specified" error message
      When the user runs "minirepro minireprodb -q ~/in.sql -f"
      Then minirepro should print "error: -f option requires an argument" error message

    @minirepro_UI
    Scenario: Query file does not exist
      Given the file "/tmp/nonefolder/in.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/nonefolder/in.sql -f ~/out.sql"
      Then minirepro should print "error: Query file /tmp/nonefolder/in.sql does not exist" error message

    @minirepro_UI
    Scenario: Database does not exist
      Given database "nonedb000" does not exist
      When the user runs "minirepro nonedb000 -q ~/test/in.sql -f ~/out.sql"
      Then minirepro error should contain database "nonedb000" does not exist

    @minirepro_core
    Scenario: Database object does not exist
      Given the file "/tmp/in.sql" exists and contains "select * from tbl_none;"
      And the table "public.tbl_none" does not exist in database "minireprodb"
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f ~/out.sql"
      Then minirepro error should contain relation "tbl_none" does not exist

    @minirepro_core
    Scenario: Query parse with multiple queries
      Given the file "/tmp/in.sql" exists and contains "select * from t1; delete from t2;"
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t1"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t2"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't1'"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't2'"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Query parse error with wrong syntax query
      Given the file "/tmp/in.sql" exists and contains "delete * from t1"
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then minirepro error should contain Error when executing function gp_dump_query_oids

    @minirepro_core
    Scenario: Dump database objects related with select query
      Given the file "/tmp/in.sql" exists and contains "select * from v1;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t1" before "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3" before "CREATE VIEW v1"
      And the output file "/tmp/out.sql" should not contain "CREATE TABLE t2"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't1'"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: a"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: b"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump cascading dependent database objects with select query
      Given the file "/tmp/in.sql" exists and contains "select * from v3;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t1" before "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3" before "CREATE VIEW v1"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t2" before "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3" before "CREATE VIEW v2"
      And the output file "/tmp/out.sql" should contain "CREATE VIEW v1" before "CREATE VIEW v3"
      And the output file "/tmp/out.sql" should contain "CREATE VIEW v2" before "CREATE VIEW v3"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't1'"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't2'"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: a"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: b"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: c"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: d"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with insert query
      Given the file "/tmp/in.sql" exists and contains "insert into t1 values(2,5);"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t1"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't1'"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: a"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: b"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with delete query
      Given the file "/tmp/in.sql" exists and contains "delete from t2;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t2"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't2'"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: c"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: d"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with update query
      Given the file "/tmp/in.sql" exists and contains "update t3 set f=1;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with create query
      Given the file "/tmp/in.sql" exists and contains "create table t0(a integer, b integer)"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should not contain "CREATE TABLE t0"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with select into query
      Given the file "/tmp/in.sql" exists and contains "select * into t0 from t3"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should not contain "CREATE TABLE t0"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with explain query
      Given the file "/tmp/in.sql" exists and contains "explain delete from t2"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t2"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't2'"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: c"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: d"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with explain analyze query
      Given the file "/tmp/in.sql" exists and contains "EXPLAIN ANALYZE select * from t1"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t1"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't1'"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: a"
      And the output file "/tmp/out.sql" should contain "Table: t1, Attribute: b"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with explain verbose query
      Given the file "/tmp/in.sql" exists and contains "EXPLAIN verbose select * from t3"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with EXPLAIN select query on view
      Given the file "/tmp/in.sql" exists and contains "EXPLAIN ANALYZE select * from v2;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t2" before "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3" before "CREATE VIEW v2"
      And the output file "/tmp/out.sql" should not contain "CREATE TABLE t1"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't2'"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: c"
      And the output file "/tmp/out.sql" should contain "Table: t2, Attribute: d"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with dummy query
      Given the file "/tmp/in.sql" exists and contains "select 1+2;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should not contain "CREATE TABLE "
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with generate_series query
      Given the file "/tmp/in.sql" exists and contains "select generate_series(1,100);"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should not contain "CREATE TABLE "
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with literal selection on table
      Given the file "/tmp/in.sql" exists and contains "select 1 from t3;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects related with boolean selection on table
      Given the file "/tmp/in.sql" exists and contains "select 1=2 from t3;"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE TABLE t3"
      And the output file "/tmp/out.sql" should contain "WHERE relname = 't3'"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: e"
      And the output file "/tmp/out.sql" should contain "Table: t3, Attribute: f"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error

    @minirepro_core
    Scenario: Dump database objects of only functions
      Given the file "/tmp/in.sql" exists and contains "SELECT select_one()"
      And the file "/tmp/out.sql" does not exist
      When the user runs "minirepro minireprodb -q /tmp/in.sql -f /tmp/out.sql"
      Then the output file "/tmp/out.sql" should exist
      And the output file "/tmp/out.sql" should contain "CREATE FUNCTION public.select_one() RETURNS integer"
      And the output file "/tmp/out.sql" should contain "LANGUAGE sql"
      And the output file "/tmp/out.sql" should contain "AS $$ select 1 $$;"
      And the output file "/tmp/out.sql" should be loaded to database "minidb_tmp" without error
      And the file "/tmp/in.sql" should be executed in database "minidb_tmp" without error