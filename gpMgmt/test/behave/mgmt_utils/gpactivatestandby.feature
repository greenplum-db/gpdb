@gpactivatestandby
Feature: gpactivatestandby

################### @demo_cluster & @concourse_cluster tests ###################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster
# The @demo_cluster tag denotes the scenario can run locally

    @concourse_cluster
    @demo_cluster
    Scenario: gpactivatestandby works
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        When there is a "heap" table "foobar" in "postgres" with data
        And the master goes down
        And the user runs gpactivatestandby with options " "
        Then gpactivatestandby should return a return code of 0
        And verify the standby master is now acting as master
        And verify that table "foobar" in "postgres" has "2190" rows
        And verify that gpstart on original master fails due to lower Timeline ID
        And clean up and revert back to original master

########################### @demo_cluster tests ###########################
# The @demo_cluster tag denotes the scenario can run locally

    @demo_cluster
    Scenario: gpactivatestandby -f forces standby master to start
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        When there is a "heap" table "foobar" in "postgres" with data
        And the master goes down
        And the standby master goes down
        And the user runs gpactivatestandby with options " "
        Then gpactivatestandby should return a return code of 2
        And the user runs gpactivatestandby with options "-f"
        Then gpactivatestandby should return a return code of 0
        And verify the standby master is now acting as master
        And verify that table "foobar" in "postgres" has "2190" rows
        And verify that gpstart on original master fails due to lower Timeline ID
        And clean up and revert back to original master

    @demo_cluster
    Scenario: gpactivatestandby should fail when given invalid data directory
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        And the user runs gpactivatestandby with options "-d invalid_directory"
        Then gpactivatestandby should return a return code of 2

    @demo_cluster
    Scenario: gpstate after running gpactivatestandby works
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        And the master goes down
        And the user runs gpactivatestandby with options " "
        Then gpactivatestandby should return a return code of 0
        And verify the standby master is now acting as master
        Then the user runs command "gpstate -s" from standby master
        And verify gpstate with options "-s" output is correct
        Then the user runs command "gpstate -Q" from standby master
        And verify gpstate with options "-Q" output is correct
        Then the user runs command "gpstate -m" from standby master
        And verify gpstate with options "-m" output is correct
        And clean up and revert back to original master
