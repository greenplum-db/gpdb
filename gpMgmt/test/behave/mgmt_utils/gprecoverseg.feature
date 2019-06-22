@gprecoverseg
Feature: gprecoverseg tests

    Scenario: incremental recovery works with tablespaces
        Given the database is running
          And a tablespace is created with data
          And user stops all primary processes
          And user can start transactions
         When the user runs "gprecoverseg -a"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid

        Given another tablespace is created with data
         When the user runs "gprecoverseg -ra"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid
          And the other tablespace is valid

    Scenario: full recovery works with tablespaces
        Given the database is running
          And a tablespace is created with data
          And user stops all primary processes
          And user can start transactions
         When the user runs "gprecoverseg -a -F"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid

        Given another tablespace is created with data
         When the user runs "gprecoverseg -ra"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid
          And the other tablespace is valid

    Scenario: gprecoverseg should not output bootstrap error on success
        Given the database is running
        And user stops all primary processes
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Running pg_rewind on required mirrors" to stdout
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout

    Scenario: gprecoverseg displays pg_basebackup progress to the user
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And user stops all mirror processes
        When user can start transactions
        And the user runs "gprecoverseg -F -a -s"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for each mirror
        And gpAdminLogs directory has no "pg_basebackup*" files
        And all the segments are running
        And the segments are synchronized

    Scenario: gprecoverseg does not display pg_basebackup progress to the user when --no-progress option is specified
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And user stops all mirror processes
        When user can start transactions
        And the user runs "gprecoverseg -F -a --no-progress"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "pg_basebackup: base backup completed" to stdout
        And gpAdminLogs directory has no "pg_basebackup*" files
        And all the segments are running
        And the segments are synchronized

    Scenario: When gprecoverseg incremental recovery uses pg_rewind to recover and an existing postmaster.pid on the killed primary segment corresponds to a non postgres process
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And user stops all primary processes
        When user can start transactions
        And the background pid is killed on "primary" segment
        And we run a sample background script to generate a pid on "primary" segment
        And we generate the postmaster.pid file with the background pid on "primary" segment
        And the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Running pg_rewind on required mirrors" to stdout
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And all the segments are running
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        And the backup pid file is deleted on "primary" segment
        And the background pid is killed on "primary" segment

    Scenario: Pid does not correspond to any running process
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And user stops all primary processes
        When user can start transactions
        And we generate the postmaster.pid file with a non running pid on the same "primary" segment
        And the user runs "gprecoverseg -a"
        And gprecoverseg should print "Running pg_rewind on required mirrors" to stdout
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And all the segments are running
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        And the backup pid file is deleted on "primary" segment

    Scenario: pg_isready functions on recovered segments
        Given the database is running
          And all the segments are running
          And the segments are synchronized
         When user stops all primary processes
          And user can start transactions

         When the user runs "gprecoverseg -a"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized

         When the user runs "gprecoverseg -ar"
         Then gprecoverseg should return a return code of 0
          And all the segments are running
          And the segments are synchronized
          And pg_isready reports all primaries are accepting connections


########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: gprecoverseg behave test requires a cluster with at least 2 hosts
        Given the database is running
        Given database "gptest" exists
        And the information of a "mirror" segment on a remote host is saved

    @concourse_cluster
    Scenario: When gprecoverseg full recovery is executed and an existing postmaster.pid on the killed primary segment corresponds to a non postgres process
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And user stops all primary processes
        When user can start transactions
        And the background pid is killed on "primary" segment
        And we run a sample background script to generate a pid on "primary" segment
        And we generate the postmaster.pid file with the background pid on "primary" segment
        And the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And gprecoverseg should print "Skipping to stop segment.* on host.* since it is not a postgres process" to stdout
        And all the segments are running
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        And the backup pid file is deleted on "primary" segment
        And the background pid is killed on "primary" segment

    @concourse_cluster
    Scenario: gprecoverseg full recovery testing
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        When user kills a "mirror" process with the saved information
        And user can start transactions
        Then the saved "mirror" segment is marked down in config
        When the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Running pg_rewind on required mirrors" to stdout
        And all the segments are running
        And the segments are synchronized

    @concourse_cluster
    Scenario: gprecoverseg with -i and -o option
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        When user kills a "mirror" process with the saved information
        And user can start transactions
        Then the saved "mirror" segment is marked down in config
        When the user runs "gprecoverseg -o failedSegmentFile"
        Then gprecoverseg should return a return code of 0
        Then gprecoverseg should print "Configuration file output to failedSegmentFile successfully" to stdout
        When the user runs "gprecoverseg -i failedSegmentFile -a"
        Then gprecoverseg should return a return code of 0
        Then gprecoverseg should print "1 segment\(s\) to recover" to stdout
        And all the segments are running
        And the segments are synchronized

    @concourse_cluster
    Scenario: gprecoverseg should not throw exception for empty input file
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        When user kills a "mirror" process with the saved information
        And user can start transactions
        Then the saved "mirror" segment is marked down in config
        When the user runs command "touch /tmp/empty_file"
        When the user runs "gprecoverseg -i /tmp/empty_file -a"
        Then gprecoverseg should return a return code of 0
        Then gprecoverseg should print "No segments to recover" to stdout
        When the user runs "gprecoverseg -a -F"
        Then all the segments are running
        And the segments are synchronized

    @concourse_cluster
    Scenario: gprecoverseg should use the same setting for data_checksums for a full recovery
        Given the database is running
        And results of the sql "show data_checksums" db "template1" are stored in the context
        # cause a full recovery AFTER a failure on a remote primary
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And the information of the corresponding primary segment on a remote host is saved
        When user kills a "primary" process with the saved information
        And user can start transactions
        Then the saved "primary" segment is marked down in config
        When the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Heap checksum setting is consistent between master and the segments that are candidates for recoverseg" to stdout
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Heap checksum setting is consistent between master and the segments that are candidates for recoverseg" to stdout
        And all the segments are running
        And the segments are synchronized
        # validate the the new segment has the correct setting by getting admin connection to that segment
        Then the saved primary segment reports the same value for sql "show data_checksums" db "template1" as was saved

    @concourse_cluster
    Scenario: incremental recovery works with tablespaces on a multi-host environment
        Given the database is running
          And a tablespace is created with data
          And user stops all primary processes
          And user can start transactions
         When the user runs "gprecoverseg -a"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid

        Given another tablespace is created with data
         When the user runs "gprecoverseg -ra"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid
          And the other tablespace is valid

    @concourse_cluster
    Scenario: full recovery works with tablespaces on a multi-host environment
        Given the database is running
          And a tablespace is created with data
          And user stops all primary processes
          And user can start transactions
         When the user runs "gprecoverseg -a -F"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid

        Given another tablespace is created with data
         When the user runs "gprecoverseg -ra"
         Then gprecoverseg should return a return code of 0
          And the segments are synchronized
          And the tablespace is valid
          And the other tablespace is valid
