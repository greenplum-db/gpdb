# NOTE: this behave suite depends on the cluster under test having at least 2 distinct hosts. See @multinode tags

@gprecoverseg
Feature: gprecoverseg tests
    @multinode
    Scenario: gprecoverseg behave test requires a cluster with at least 2 hosts
        Given the database is running
        Given database "gptest" exists
        And the information of a "mirror" segment on a remote host is saved

    Scenario: gprecoverseg should not output bootstrap error on success
        Given the database is running
        And user kills a primary postmaster process
        And user can start transactions
        # WALREP_FIXME: Change to incremental once pg_rewind is introduced into gprecoverseg
        When the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout

    Scenario: Pid corresponds to a non postgres process
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And user kills a primary postmaster process
        When user can start transactions
        And the background pid is killed on "primary" segment
        And we run a sample background script to generate a pid on "primary" segment
        And we generate the postmaster.pid file with the background pid on "primary" segment
        # WALREP_FIXME: Change to incremental once pg_rewind is introduced into gprecoverseg
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

    Scenario: Pid does not correspond to any running process
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And user kills a primary postmaster process
        When user can start transactions
        And we generate the postmaster.pid file with a non running pid on the same "primary" segment
        # WALREP_FIXME: Change to incremental once pg_rewind is introduced into gprecoverseg
        And the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And all the segments are running
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        And the backup pid file is deleted on "primary" segment

    @multinode
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
        And all the segments are running
        And the segments are synchronized

    @multinode
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

    @multinode
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

    @multinode
    @gprecoverseg_checksums
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
