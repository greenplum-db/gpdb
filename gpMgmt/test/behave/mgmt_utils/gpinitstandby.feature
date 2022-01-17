@gpinitstandby
Feature: Tests for gpinitstandby feature

    Scenario: gpinitstandby with -n option (manually start standby coordinator)
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        And the user runs gpinitstandby with options "-n"
        And gpinitstandby should print "Standy coordinator is already up and running" to stdout
        When the standby coordinator goes down
        And the user runs gpinitstandby with options "-n"
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        And gpinitstandby should print "Successfully started standby coordinator" to stdout

    Scenario: gpinitstandby fails if given same host and port as coordinator segment
        Given the database is running
        And the standby is not initialized
        When the user initializes a standby on the same host as coordinator with same port
        Then gpinitstandby should return a return code of 2
        And gpinitstandby should print "cannot create standby on the same host and port" to stdout

    Scenario: gpinitstandby fails if given same host and datadir as coordinator segment
        Given the database is running
          And the standby is not initialized
         When the user initializes a standby on the same host as coordinator and the same data directory
         Then gpinitstandby should return a return code of 2
          And gpinitstandby should print "coordinator data directory exists" to stdout
          And gpinitstandby should print "use -S and -P to specify a new data directory and port" to stdout

    Scenario: gpinitstandby exclude dirs
        Given the database is running
        And the standby is not initialized
        And the file "log/testfile" exists under coordinator data directory
        And the file "db_dumps/testfile" exists under coordinator data directory
        And the file "promote/testfile" exists under coordinator data directory
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        And the file "log/testfile" does not exist under standby coordinator data directory
        And the file "db_dumps/testfile" does not exist under standby coordinator data directory
        And the file "promote/testfile" does not exist under standby coordinator data directory
        And the file "pg_hba.conf.bak" does not exist under standby coordinator data directory
        ## maybe clean up the directories created in the coordinator data directory

    Scenario: gpstate -f shows standby coordinator information after running gpinitstandby
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
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

    Scenario: gpinitstandby requires additional confirmation prompt when a segment host is unreachable
        Given the database is running
          And the standby is not initialized
          And the host for the primary on content 0 is made unreachable

         When the user runs gpinitstandby and accepts the unreachable host prompt

         Then gpinitstandby should return a return code of 0
          And gpinitstandby should print "If you continue with initialization, pg_hba.conf files on these hosts will not be updated." to stdout
          And the cluster is returned to a good state

    Scenario: gpinitstandby exits if confirmation is not given when a segment host is unreachable
        Given the database is running
          And the standby is not initialized
          And the host for the primary on content 0 is made unreachable

         When the user runs gpinitstandby and rejects the unreachable host prompt

         Then gpinitstandby should return a return code of 2
          And gpinitstandby should print "If you continue with initialization, pg_hba.conf files on these hosts will not be updated." to stdout
          And gpinitstandby should print "Error initializing standby coordinator: User canceled" to stdout
          And the cluster is returned to a good state

    Scenario: gpinitstandby exits if -a is used when a segment host is unreachable
        Given the database is running
          And the standby is not initialized
          And the host for the primary on content 0 is made unreachable

         When the user runs gpinitstandby with options " "

         Then gpinitstandby should return a return code of 2
          And gpinitstandby should print "If you want to continue with initialization, re-run gpinitstandby without the -a option." to stdout
          And gpinitstandby should print "Error initializing standby coordinator: Unable to proceed while one or more hosts are unreachable" to stdout
          And the cluster is returned to a good state

    Scenario: gpinitstandby creates the standby with default data_checksums on
        Given the database is running
        And the standby is not initialized
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        Then gpconfig should print "Values on all segments are consistent" to stdout
        Then gpconfig should print "Coordinator value: on" to stdout
        Then gpconfig should print "Segment     value: on" to stdout
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
        And gpconfig should print "Coordinator value: off" to stdout
        And gpconfig should print "Segment     value: off" to stdout
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And gpinitstandby should not print "Traceback" to stdout
        When the user runs pg_controldata against the standby data directory
        Then pg_controldata should print "Data page checksum version:           0" to stdout

    Scenario: gpinitstandby does not add entries for 127.0.0.x and ::1 in pg_hba.conf
        Given the database is running
        And the standby is not initialized
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And verify that the file "pg_hba.conf" in the coordinator data directory has "no" line starting with "host.*replication.*(127.0.0.1|::1).*trust"

    @backup_restore_bashrc
    Scenario: gpinitstandby should not throw error when banner exists on the host
        Given the database is running
        And the standby is not initialized
        When the user sets banner on host
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0

    @concourse_cluster
    Scenario: gpinitstandby should create pg_hba entry to segment primary
        Given the database is not running
        And a working directory of the test as '/tmp/gpinitstandby'
        And a cluster is created with mirrors on "mdw" and "sdw1"
        And the standby is not initialized
        When running gpinitstandby on host "mdw" to create a standby on host "sdw1"
        Then gpinitstandby should return a return code of 0
        And verify the standby coordinator entries in catalog
        And verify that pg_hba.conf file has "standby" entries in each segment data directories
