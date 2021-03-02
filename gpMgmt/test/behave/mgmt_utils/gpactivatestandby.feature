@gpactivatestandby
Feature: gpactivatestandby

    Scenario: gpactivatestandby works
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        When there is a "heap" table "foobar" in "postgres" with data
        And the coordinator goes down
        And the user runs gpactivatestandby with options " "
        Then gpactivatestandby should return a return code of 0
        And verify the standby coordinator is now acting as coordinator
        And verify that table "foobar" in "postgres" has "2190" rows
        And verify that gpstart on original coordinator fails due to lower Timeline ID
        And clean up and revert back to original coordinator

    Scenario: gpactivatestandby -f forces standby coordinator to start
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        When there is a "heap" table "foobar" in "postgres" with data
        And the coordinator goes down
        And the standby coordinator goes down
        And the user runs gpactivatestandby with options " "
        Then gpactivatestandby should return a return code of 2
        And the user runs gpactivatestandby with options "-f"
        Then gpactivatestandby should return a return code of 0
        And verify the standby coordinator is now acting as coordinator
        And verify that table "foobar" in "postgres" has "2190" rows
        And verify that gpstart on original coordinator fails due to lower Timeline ID
        And clean up and revert back to original coordinator

    Scenario: gpactivatestandby should fail when given invalid data directory
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        And the user runs gpactivatestandby with options "-d invalid_directory"
        Then gpactivatestandby should return a return code of 2

    Scenario: gpstate after running gpactivatestandby works
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        And the coordinator goes down
        And the user runs gpactivatestandby with options " "
        Then gpactivatestandby should return a return code of 0
        And verify the standby coordinator is now acting as coordinator
        Then the user runs command "gpstate -s" from standby coordinator
        And verify gpstate with options "-s" output is correct
        Then the user runs command "gpstate -Q" from standby coordinator
        And verify gpstate with options "-Q" output is correct
        Then the user runs command "gpstate -m" from standby coordinator
        And verify gpstate with options "-m" output is correct
        And clean up and revert back to original coordinator

    Scenario: tablespaces work
        Given the database is running
          And the standby is not initialized
          And a tablespace is created with data
         When the user runs gpinitstandby with options " "
         Then gpinitstandby should return a return code of 0
          And verify the standby coordinator entries in catalog

         When the coordinator goes down
         Then the user runs gpactivatestandby with options " "
          And gpactivatestandby should return a return code of 0
          And verify the standby coordinator is now acting as coordinator
          And the tablespace is valid on the standby coordinator
          And clean up and revert back to original coordinator

########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: tablespaces work on a multi-host environment
        Given the database is running
          And the standby is not initialized
          And a tablespace is created with data
         When the user runs gpinitstandby with options " "
         Then gpinitstandby should return a return code of 0
          And verify the standby coordinator entries in catalog

         When the coordinator goes down
         Then the user runs gpactivatestandby with options " "
          And gpactivatestandby should return a return code of 0
          And verify the standby coordinator is now acting as coordinator
          And the tablespace is valid on the standby coordinator
          And clean up and revert back to original coordinator
