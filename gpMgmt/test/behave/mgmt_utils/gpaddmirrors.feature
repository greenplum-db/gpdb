@gpaddmirrors
Feature: Tests for gpaddmirrors

    Scenario: gpaddmirrors with a default master data directory
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors

    @gpaddmirrors_temp_directory
    Scenario: gpaddmirrors with a given master data directory [-d <master datadir>]
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And gpaddmirrors adds mirrors with temporary data dir
        Then verify the database has mirrors
