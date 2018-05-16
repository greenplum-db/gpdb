@gpaddmirrors
Feature: Tests for gpaddmirrors

    Scenario: gpaddmirrors with a default master data directory
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the user runs command "rm -rf /tmp/gpaddmirrors/*"
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors

    @gpaddmirrors_temp_directory
    Scenario: gpaddmirrors with a given master data directory [-d <master datadir>]
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the user runs command "rm -rf /tmp/gpaddmirrors/*"
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And gpaddmirrors adds mirrors with temporary data dir
        Then verify the database has mirrors

    @gpaddmirrors_workload
    Scenario: gpaddmirrors when the primaries have data
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the user runs command "rm -rf /tmp/gpaddmirrors/*"
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there is a "heap" table "public.heap_table" in "gptest" with "100" rows
        And there is a "ao" table "public.ao_table" in "gptest" with "100" rows
        And there is a "co" table "public.co_table" in "gptest" with "100" rows
        And gpaddmirrors adds mirrors with temporary data dir
        And an FTS probe is triggered
        And the segments are synchronized
        When user kills all primary processes
        And user can start transactions
        Then verify that there is a "heap" table "public.heap_table" in "gptest" with "100" rows
        Then verify that there is a "ao" table "public.ao_table" in "gptest" with "100" rows
        Then verify that there is a "co" table "public.co_table" in "gptest" with "100" rows
