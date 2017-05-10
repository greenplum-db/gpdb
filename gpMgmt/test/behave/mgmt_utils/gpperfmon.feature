@gpperfmon
Feature: gpperfmon
    """
    Quantum controls how frequently text files get their data put in external tables.
    Note that only certain values are possible: 10, 15, 20, 30, 60.
    In this test setup, it's within the second quantum that the files are processed
    perhaps because of some initialization.
    So for quantum = 10, an initial query-log-to-external-table phase will take up to 20 seconds

    Harvest_interval controls how frequently the perfmon external tables are brought into the heap tables

    Important: for this behave to succeed on macOS, see steps in gpdb/gpAux/gpperfmon/README
    """
    @gpperfmon_install
    Scenario: install gpperfmon
        Given the environment variable "PGDATABASE" is set to "template1"
        Given the database "gpperfmon" does not exist
        When the user runs "gpperfmon_install --port 15432 --enable --password foo"
        Then gpperfmon_install should return a return code of 0
        Then verify that the last line of the master postgres configuration file contains the string "gpperfmon_log_alert_level=warning"
        And verify that there is a "heap" table "database_history" in "gpperfmon"

    @gpperfmon_run
    Scenario: run gpperfmon
        Given the environment variable "PGDATABASE" is set to "template1"
        Given the database "gpperfmon" does not exist
        Then gpperfmon is configured and running in qamode

    @gpperfmon_database_history
    Scenario: gpperfmon adds to database_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "database_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM database_history" is "true"

    @gpperfmon_query_history
    Scenario: gpperfmon adds to query_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "queries_history" tables in "gpperfmon"
        And the user runs "psql -c 'select 1;' template1"
        Then psql should return a return code of 0
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history" is "true"

    @gpperfmon_query_history
    Scenario: gpperfmon adds to query_history table with carriage returns
        Given gpperfmon is configured and running in qamode
        When the user truncates "queries_history" tables in "gpperfmon"
        And execute following sql in db "gpperfmon" and store result in the context
            """
            --some comment
            SELECT 1
            ;
            """
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history where query_text like '--some%'" is "true"
        When execute following sql in db "gpperfmon" and store result in the context
            """
            SELECT query_text FROM queries_history where query_text like '--some%'
            """
        Then validate that first column of first stored row has "3" lines of raw output

    @gpperfmon_system_history
    Scenario: gpperfmon adds to system_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "system_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM system_history" is "true"

    @gpperfmon_log_alert_history
    Scenario: gpperfmon adds to log_alert_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "log_alert_history" tables in "gpperfmon"
        And the user runs "psql non_existing_database"
        Then psql should return a return code of 2
        When the latest gpperfmon gpdb-alert log is copied to a file with a fake (earlier) timestamp
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM log_alert_history" is "true"
        And the file with the fake timestamp no longer exists

    @gpperfmon_segment_history
    Scenario: gpperfmon adds to segment_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "segment_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM segment_history" is "true"

    @gpperfmon_diskspace_history
    Scenario: gpperfmon adds to diskspace_history table
        Given gpperfmon is configured and running in qamode
        When the user truncates "diskspace_history" tables in "gpperfmon"
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM diskspace_history" is "true"

    @gpperfmon_skew_rows
    Scenario: gpperfmon detects row skew
        Given gpperfmon is configured and running in qamode
        Given database "gptest" is dropped and recreated
        Given the user runs "psql gptest -c 'create table test_row_skew( id int, val int ) DISTRIBUTED BY ( id );'"
        Then psql should return a return code of 0
        When the user truncates "queries_history" tables in "gpperfmon"
        # 1 million skewed rows too few to detect skew, but 10M seems to let gpperfmon detect row skew. Why?
        And the user runs "psql gptest -c 'INSERT INTO test_row_skew SELECT 1, i FROM generate_series(1,10000000) AS i;'"
        Then psql should return a return code of 0
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history where skew_rows > 0" is "true"
