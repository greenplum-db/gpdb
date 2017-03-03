@gpstop
Feature: Basic gpstop testing

    @dca
    Scenario: gpstop should not dump core files
        Given the database is running
        And the core dump directory is stored
        And the number of core files "before" running "gpstop" is stored 
        When the user runs "gpstop -a"
        Then gpstop should return a return code of 0
        And the number of core files "after" running "gpstop" is stored
        And the number of core files is the same
