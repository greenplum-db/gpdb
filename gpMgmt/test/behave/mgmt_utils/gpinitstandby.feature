@gpinitstandby
Feature: Tests for gpinitstandby feature

    Scenario: gpinitstandby with -n option (manually start standby master)
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        And the user runs gpinitstandby with options "-n"
        And gpinitstandby should print "Standy master is already up and running" to stdout
        When the standby master goes down
        And the user runs gpinitstandby with options "-n"
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        And gpinitstandby should print "Successfully started standby master" to stdout

    Scenario: gpinitstandby fails if given same host and port as master segment
        Given the database is running
        And the standby is not initialized
        When the user initializes a standby on the same host as master with same port
        Then gpinitstandby should return a return code of 2
        And gpinitstandby should print "cannot create standby on the same host and port" to stdout

    Scenario: gpinitstandby fails if given same host and datadir as master segment
        Given the database is running
          And the standby is not initialized
         When the user initializes a standby on the same host as master and the same data directory
         Then gpinitstandby should return a return code of 2
          And gpinitstandby should print "master data directory exists" to stdout
          And gpinitstandby should print "use -S and -P to specify a new data directory and port" to stdout

    Scenario: gpinitstandby exclude dirs
        Given the database is running
        And the standby is not initialized
        And the file "pg_log/testfile" exists under master data directory
        And the file "db_dumps/testfile" exists under master data directory
        And the file "gpperfmon/data/testfile" exists under master data directory
        And the file "gpperfmon/logs/testfile" exists under master data directory
        And the file "promote/testfile" exists under master data directory
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        And the file "pg_log/testfile" does not exist under standby master data directory
        And the file "db_dumps/testfile" does not exist under standby master data directory
        And the file "gpperfmon/data/testfile" does not exist under standby master data directory
        And the file "gpperfmon/logs/testfile" does not exist under standby master data directory
        And the file "promote/testfile" does not exist under standby master data directory
        ## maybe clean up the directories created in the master data directory

    Scenario: gpstate -f shows standby master information after running gpinitstandby
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby master entries in catalog
        Then the user runs command "gpstate -f"
        And verify gpstate with options "-f" output is correct

    Scenario: gpinitstandby should not throw stacktrace when $GPHOME/share directory is non-writable
        Given the database is running
        And the standby is not initialized
        And "$GPHOME/share" has its permissions set to "555"
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        And rely on environment.py to restore path permissions

    Scenario: gpinitstandby creates the standby with default data_checksums on
        Given the database is running
        And the standby is not initialized
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        Then gpconfig should print "Values on all segments are consistent" to stdout
        Then gpconfig should print "Master  value: on" to stdout
        Then gpconfig should print "Segment value: on" to stdout
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        When the user runs pg_controldata against the standby data directory
        Then pg_controldata should print "Data page checksum version:           1" to stdout

    Scenario: gpinitstandby creates the standby with default data_checksums off
        Given the database is initialized with checksum "off"
        And the standby is not initialized
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        And gpconfig should print "Values on all segments are consistent" to stdout
        And gpconfig should print "Master  value: off" to stdout
        And gpconfig should print "Segment value: off" to stdout
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        When the user runs pg_controldata against the standby data directory
        Then pg_controldata should print "Data page checksum version:           0" to stdout

