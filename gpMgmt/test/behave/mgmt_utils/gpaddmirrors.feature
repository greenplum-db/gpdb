@gpaddmirrors
Feature: Tests for gpaddmirrors

    Scenario: gpaddmirrors with a default master data directory
        Given the database is initialized with no mirrored segments in the configuration
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors

    @gpaddmirrors_temp_directory
    Scenario: gpaddmirrors with a given master data directory [-d <master datadir>]
        Given the database is initialized with no mirrored segments in the configuration
        And gpaddmirrors adds mirrors with temporary data dir
        Then verify the database has mirrors
