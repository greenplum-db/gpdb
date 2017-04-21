@gpperfmon
Feature: gpperfmon

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
        # important: to succeed on macOS, see steps in gpdb/gpAux/gpperfmon/README
        Given the environment variable "PGDATABASE" is set to "template1"
        Given the database "gpperfmon" does not exist
        When the user runs "gpperfmon_install --port 15432 --enable --password foo"
        Then gpperfmon_install should return a return code of 0
        When the user runs command "pkill postgres"
        Then wait until the process "postgres" goes down
        When the user runs "gpstart -a"
        Then gpstart should return a return code of 0
        And verify that a role "gpmon" exists in database "gpperfmon"
        And verify that the last line of the master postgres configuration file contains the string "gpperfmon_log_alert_level=warning"
        And verify that there is a "heap" table "database_history" in "gpperfmon"
        Then wait until the process "gpmmon" is up
        And wait until the process "gpsmon" is up

    @gpperfmon_query_history
    Scenario: gpperfmon adds to query_history table
        # important: to succeed on macOS, see steps in gpdb/gpAux/gpperfmon/README
        Given the database "gpperfmon" does not exist
        When the user runs command "rm -rf $MASTER_DATA_DIRECTORY/gpperfmon/data"
        When the user runs "gpperfmon_install --port 15432 --enable --password foo"
        Then gpperfmon_install should return a return code of 0
        And verify that there is a "heap" table "queries_history" in "gpperfmon"
        When the user runs command "echo 'qamode = 1' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
        When the user runs command "echo 'verbose = 1' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
        When the user runs command "echo 'min_query_time = 2' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
        # Quantum controls how frequently text files get their data put in external tables.
        # Note that only certain values are possible: 10, 15, 20, 30, 60.
        # In this test setup, it's within the second quantum that the files are processed
        # perhaps because of some initialization.
        # So for quantum = 10, an initial query-log-to-external-table phase will take up to 20 seconds
        When the user runs command "echo 'quantum = 10' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
        # Harvest_interval controls how frequently the perfmon external tables are brought into the heap tables
        When the user runs command "echo 'harvest_interval = 5' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
        When the user runs command "pkill postgres"
        Then wait until the process "postgres" goes down
        And the user runs "gpstart -a"
        Then gpstart should return a return code of 0
        And wait until the process "gpmmon" is up
        And wait until the process "gpsmon" is up
        When the user runs "psql -c 'select pg_sleep(3);' template1"
        Then psql should return a return code of 0
        Then wait until the results from boolean sql "SELECT count(*) > 0 FROM queries_history" is "true"


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
