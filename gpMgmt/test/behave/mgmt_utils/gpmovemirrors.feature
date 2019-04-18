@gpmovemirrors
Feature: Tests for gpmovemirrors

    Scenario: gpmovemirrors fails with totally malformed input file
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'malformed' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 3

    Scenario: gpmovemirrors fails with bad host in input file
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'badhost' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 3

    Scenario: gpmovemirrors fails with invalid option parameter
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'good' gpmovemirrors file is created
        When the user runs gpmovemirrors with additional args "--invalid-option"
        Then gpmovemirrors should return a return code of 2

    Scenario: gpmovemirrors can change the location of mirrors within a single host
        Given a standard local demo cluster is created
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'good' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 0
        And verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirrors are recognized after a restart

    Scenario: gpmovemirrors can change the port of mirrors within a single host
        Given a standard local demo cluster is created
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'samedir' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 0
        And verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirrors are recognized after a restart

    Scenario: gpmovemirrors gives a warning when passed identical attributes for new and old mirrors
        Given a standard local demo cluster is created
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'identicalAttributes' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 0
	And gpmovemirrors should print a "request to move a mirror with identical attributes" warning
	And verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirrors are recognized after a restart

    Scenario: tablespaces work
        Given a standard local demo cluster is created
          And a tablespace is created with data
          And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
          And a 'good' gpmovemirrors file is created
         When the user runs gpmovemirrors
         Then gpmovemirrors should return a return code of 0
          And verify the database has mirrors
          And all the segments are running
          And the segments are synchronized
          And verify that mirrors are recognized after a restart
          And the tablespace is valid

########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: gpmovemirrors can change from group mirroring to spread mirroring
        Given verify that mirror segments are in "group" configuration
        And a sample gpmovemirrors input file is created in "spread" configuration
        When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_spread"
        Then gpmovemirrors should return a return code of 0
        # Verify that mirrors are functional in the new configuration
        Then verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirror segments are in "spread" configuration
        And verify that mirrors are recognized after a restart

    @concourse_cluster
    Scenario: gpmovemirrors can change from spread mirroring to group mirroring
        Given verify that mirror segments are in "spread" configuration
        And a sample gpmovemirrors input file is created in "group" configuration
        When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_group"
        Then gpmovemirrors should return a return code of 0
        # Verify that mirrors are functional in the new configuration
        Then verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirror segments are in "group" configuration
        And verify that mirrors are recognized after a restart

    @concourse_cluster
    Scenario: tablespaces work on a multi-host environment
        Given verify that mirror segments are in "group" configuration
          And a tablespace is created with data
          And a sample gpmovemirrors input file is created in "spread" configuration
         When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_spread"
         Then gpmovemirrors should return a return code of 0
          And verify the database has mirrors
          And all the segments are running
          And the segments are synchronized
          And verify that mirrors are recognized after a restart
          And the tablespace is valid

         When user stops all primary processes
          And user can start transactions
         Then the tablespace is valid
