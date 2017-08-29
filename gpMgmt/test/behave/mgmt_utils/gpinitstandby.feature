@gpinitstandby
Feature: Tests for gpinitstandby feature
    Scenario: gpinitstandby should not throw stacktrace when $GPHOME/share directory is non-writable
        Given the database is running
        And the standby is not initialized
        And all the segments are running
        And the segments are synchronized
        And "$GPHOME/share" has its permissions set to "555"
        When the user initializes a standby on the same host as master
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        And rely on environment.py to restore path permissions

    Scenario: gpinitstandby creates the standby with default data_checksums on (standby on same host as master)
        Given the database is running
        And the standby is not initialized
        And all the segments are running
        And the segments are synchronized
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        Then gpconfig should print "Values on all segments are consistent" to stdout
        Then gpconfig should print "Master  value: on" to stdout
        Then gpconfig should print "Segment value: on" to stdout
        When the user initializes a standby on the same host as master
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        When the user runs pg_controldata against the standby data directory
        Then pg_controldata should print "Data page checksum version:           1" to stdout

    @gpinitstandby_checksum_off
    Scenario: gpinitstandby creates the standby with default data_checksums off (standby on same host as master)
        Given the database is initialized with checksum "off"
        And the standby is not initialized
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        And gpconfig should print "Values on all segments are consistent" to stdout
        And gpconfig should print "Master  value: off" to stdout
        And gpconfig should print "Segment value: off" to stdout
        And all the segments are running
        And the segments are synchronized
        When the user initializes a standby on the same host as master
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        When the user runs pg_controldata against the standby data directory
        Then pg_controldata should print "Data page checksum version:           0" to stdout
