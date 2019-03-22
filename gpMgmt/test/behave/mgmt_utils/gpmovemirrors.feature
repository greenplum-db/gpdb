@gpmovemirrors
Feature: Tests for gpmovemirrors

    #  @demo_cluster  tests
    # To run these tests locally, you should have a local demo cluster created, and
    # started.  This test will modify the original local demo cluster.  This test
    # will, as a side effect, destroy the current contents of /tmp/gpmovemirrors and
    # replace it with data as used in this test.
    #
    #  @concourse_cluster tests
    # These tests require a cluster in a specified configuration, so are not
    # expected to work locally.

    @demo_cluster
    Scenario: gpmovemirrors fails with totally malformed input file
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'malformed' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 3

    @demo_cluster
    Scenario: gpmovemirrors fails with bad host in input file
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'badhost' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 3

    @demo_cluster
    Scenario: gpmovemirrors fails with invalid option parameter
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'good' gpmovemirrors file is created
        When the user runs gpmovemirrors with additional args "--invalid-option"
        Then gpmovemirrors should return a return code of 2

    @demo_cluster
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
        And the user runs "gpstop -aqM fast"
