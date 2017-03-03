@gpperfmon
Feature: gpperfmon 

    Scenario: drop old partition 
        When the user runs command "echo 'partition_age = 4' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"   
        And the user runs command "test/behave/mgmt_utils/steps/data/add_par.sh"
        And the user runs command "gpstop -ra"

        When execute following sql in db "gpperfmon" and store result in the context
            """
            SELECT count(*) FROM pg_partitions WHERE tablename = 'diskspace_history';
            """
        Then validate that following rows are in the stored rows
            | count |
            | 5     | 
