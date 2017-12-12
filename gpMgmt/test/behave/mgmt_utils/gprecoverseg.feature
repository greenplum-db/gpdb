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
        When the user runs "gprecoverseg -a"
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
        And the user runs "gprecoverseg -a"
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
        And the user runs "gprecoverseg -a"
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
    Scenario: gprecoverseg full recovery testing, with gpfaultinjector putting cluster into change tracking
        Given the database is running
        And the database "gptest1" does not exist
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" with the saved "mirror" segment option
        Given database "gptest1" exists
        Then the saved mirror segment is marked down in config
        And the saved mirror segment process is still running on that host
        And user can start transactions
        When the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running

    @multinode
    Scenario: gprecoverseg with -i and -o option
        Given the database is running
        And the database "gptest1" does not exist
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" with the saved "mirror" segment option
        Given database "gptest1" exists
        Then the saved mirror segment is marked down in config
        And the saved mirror segment process is still running on that host
        And user can start transactions
        When the user runs "gprecoverseg -o failedSegmentFile"
        Then gprecoverseg should return a return code of 0
        Then gprecoverseg should print "Configuration file output to failedSegmentFile successfully" to stdout
        When the user runs "gprecoverseg -i failedSegmentFile -a"
        Then gprecoverseg should return a return code of 0
        Then gprecoverseg should print "1 segment\(s\) to recover" to stdout

    @multinode
    @fail_on_corrupted_change_tracking
    Scenario: gprecoverseg fails on corrupted change tracking logs, must run full recovery
        Given the database is running
        And the database "gptest1" does not exist
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And the information of the corresponding primary segment on a remote host is saved

        When user runs the command "gpfaultinjector -f filerep_consumer  -m async -y fault" with the saved "mirror" segment option
        Given database "gptest1" exists
        Then the saved mirror segment is marked down in config

        When user runs the command "gpfaultinjector -y skip -f change_tracking_disable" with the saved "primary" segment option
        Given the database "gptest1" does not exist
        Then wait until the segment state of the corresponding primary goes in ChangeTrackingDisabled

        When the user runs "gprecoverseg -a"
        Then gprecoverseg should print "in change tracking disabled state, need to run recoverseg with -F option" to stdout
        And the saved mirror segment is marked down in config

        When the user runs "gprecoverseg -a -F"
        Then all the segments are running
        And the segments are synchronized
        And user runs the command "gpfaultinjector -y reset -f change_tracking_disable" with the saved "primary" segment option

    @multinode
    Scenario: gprecoverseg should not throw exception for empty input file
        Given the database is running
        And the database "gptest1" does not exist
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" with the saved "mirror" segment option
        Given database "gptest1" exists
        Then the saved mirror segment is marked down in config
        And the saved mirror segment process is still running on that host
        And user can start transactions
        When the user runs command "touch /tmp/empty_file"
        When the user runs "gprecoverseg -i /tmp/empty_file -a"
        Then gprecoverseg should return a return code of 0
        Then gprecoverseg should print "No segments to recover" to stdout
        When the user runs "gprecoverseg -a -F"
        Then all the segments are running
        And the segments are synchronized

    Scenario: gprecoverseg does not recover segments with persistent rebuild inconsistencies
        Given the database is running
        And the database "gptest1" does not exist
        And all the segments are running
        And the segments are synchronized
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" on segment "0"
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" on segment "1"
        Given database "gptest1" exists
        Then the mirror with content id "0" is marked down in config
        Then the mirror with content id "1" is marked down in config
        And segment with content "0" has persistent tables that were rebuilt with mirrors disabled
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        Then verify that segment with content "0" is not recovered
        Then verify that segment with content "1" is recovered
        Then delete extra tid persistent table entries on cid "0"
        When the user runs "gprecoverseg -a -F"
        Then all the segments are running
        And the segments are synchronized

    Scenario: gprecoverseg -r should not hang when some segments are not yet synchronized
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on any host is saved
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y reset" on segment "0"
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" on segment "0"
        Given database "gptest1" exists
        Then the mirror with content id "0" is marked down in config
        When the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        Given at least one segment is resynchronized
        When the user runs "gprecoverseg -r -a"
        Then gprecoverseg should print "Some segments are not yet synchronized" to stdout
        Then gprecoverseg should return a return code of 2
        And the segments are synchronized
        When the user runs "gprecoverseg -r -a"
        Then gprecoverseg should return a return code of 0
        Then all the segments are running
        And the segments are synchronized

    @multinode
    @gprecoverseg_checksums
    Scenario: gprecoverseg should use the same setting for data_checksums for a full recovery
        Given the database is running
        And the database "gptest1" does not exist
        And results of the sql "show data_checksums" db "template1" are stored in the context
        # cause a full recovery AFTER an injected failure on a remote primary
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And the information of the corresponding primary segment on a remote host is saved
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y reset" with the saved "primary" segment option
        And user runs the command "gpfaultinjector  -f filerep_consumer  -m async -y fault" with the saved "primary" segment option
        Given database "gptest1" exists
        Then the saved mirror segment is marked down in config
        And the saved mirror segment process is still running on that host
        And user can start transactions
        When the user runs "gprecoverseg -F -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Heap checksum setting is consistent between master and the segments that are candidates for recoverseg" to stdout
        And all the segments are running
        # validate the the new segment has the correct setting by getting admin connection to that segment
        Then the saved primary segment reports the same value for sql "show data_checksums" db "template1" as was saved
