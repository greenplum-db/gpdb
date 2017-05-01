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

#    todo this test may have never run. Is it valid? Worthy of fixing?
#    Scenario: drop old partition
#        When the user runs command "echo 'partition_age = 4' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
#        And the user runs command "test/behave/mgmt_utils/steps/data/add_par.sh"
#        And the user runs command "gpstop -ra"
#
#        When execute following sql in db "gpperfmon" and store result in the context
#            """
#            SELECT count(*) FROM pg_partitions WHERE tablename = 'diskspace_history';
#            """
#        Then validate that following rows are in the stored rows
#            | count |
#            | 5     |
