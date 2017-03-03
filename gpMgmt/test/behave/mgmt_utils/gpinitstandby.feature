@gpinitstandby
Feature: Tests for gpinitstandby feature
    Scenario: gpinitstandby should not throw stacktrace when $GPHOME/share directory is non-writable 
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And "$GPHOME/share" has "555" permissions
        And the standby is not initialized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print Traceback to stdout
        And "$GPHOME/share" has "777" permissions
        And the standby is initialized if required
