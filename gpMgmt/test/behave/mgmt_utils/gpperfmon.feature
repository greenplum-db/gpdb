@gpperfmon
Feature: gpperfmon

    @gpperfmon_install
    Scenario: install gpperfmon
        Given the database "gpperfmon" does not exist
        When the user runs "gpperfmon_install --port 15432 --enable --password foo"
        Then gpperfmon_install should return a return code of 0
        Then verify that the last line of the master postgres configuration file contains the string "gpperfmon_log_alert_level=warning"
        And verify that there is a "heap" table "database_history" in "gpperfmon"

    @gpperfmon_run
    Scenario: run gpperfmon
        # important: to succeed on macOS, see steps in gpdb/gpAux/gpperfmon/README
        Given the database "gpperfmon" does not exist
        When the user runs "gpperfmon_install --port 15432 --enable --password foo"
        Then gpperfmon_install should return a return code of 0
        When the user runs command "pkill postgres"
        And waiting "5" seconds
        And the user runs "gpstart -a"
        Then gpstart should return a return code of 0
        Then verify that a role "gpmon" exists in database "gpperfmon"
        Then verify that the last line of the master postgres configuration file contains the string "gpperfmon_log_alert_level=warning"
        And verify that there is a "heap" table "database_history" in "gpperfmon"
        When the user runs command "pgrep gpmmon"
        Then pgrep should return a return code of 0
        When waiting "5" seconds
        When the user runs command "pgrep gpsmon"
        Then pgrep should return a return code of 0

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
