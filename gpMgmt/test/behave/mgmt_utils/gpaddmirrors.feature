@gpaddmirrors
Feature: Tests for gpaddmirrors
    Scenario: tablespaces work
        Given the cluster is generated with "3" primaries only
          And a tablespace is created with data
         When gpaddmirrors adds 3 mirrors
          And an FTS probe is triggered
          #gpaddmirrors triggers full recovery where old replication slot is dropped and new one is created
          And verify replication slot internal_wal_replication_slot is available on all the segments
          And the segments are synchronized
         Then verify the database has mirrors
          And the tablespace is valid

         When user stops all primary processes
          And user can start transactions
         Then the tablespace is valid

    Scenario Outline: limits number of parallel processes correctly
        Given the cluster is generated with "3" primaries only
        And a tablespace is created with data
        When gpaddmirrors adds 3 mirrors with additional args "<args>"
        Then gpaddmirrors should only spawn up to <coordinator_workers> workers in WorkerPool
        And check if gpaddmirrors ran "$GPHOME/sbin/gpsegsetuprecovery.py" 1 times with args "-b <segHost_workers>"
        And check if gpaddmirrors ran "$GPHOME/sbin/gpsegrecovery.py" 1 times with args "-b <segHost_workers>"
        And check if gpaddmirrors ran "$GPHOME/sbin/gpsegrecovery.py" 1 times with args "-b <segHost_workers>"
        And an FTS probe is triggered
        And the segments are synchronized
        And verify the database has mirrors
        #gpaddmirrors triggers full recovery where old replication slot is dropped and new one is created
        And verify replication slot internal_wal_replication_slot is available on all the segments
        And the tablespace is valid
        And user stops all primary processes
        And user can start transactions
        And the tablespace is valid
    Examples:
        | args      | coordinator_workers | segHost_workers |
        | -B 1 -b 1 |  1                  |  1              |
        | -B 2 -b 1 |  2                  |  1              |
        | -B 1 -b 2 |  1                  |  2              |

    Scenario: gpaddmirrors fails for recovery setup errors
        Given the cluster is generated with "3" primaries only
        When gpaddmirrors adds 3 mirrors with one mirror's datadir not empty
        Then gpaddmirrors should return a return code of 2
        And gpaddmirrors should print "Failed to setup recovery for the following segments" to stdout
        And gpaddmirrors should print "gpaddmirrors error" to stdout
        And gpaddmirrors should print "Failed to setup recovery for the following segments" to stdout
        And gpaddmirrors should not print "Initiating segment recovery" to stdout
        #TODO assert for actual hostname, port etc.
        And gpaddmirrors should print " hostname: .*; port: .*; error: for segment with port .*: Segment directory .*" to stdout
        And verify the database has no mirrors
        And user can start transactions

        When gpaddmirrors adds 3 mirrors
        Then gpaddmirrors should return a return code of 0
        And verify the database has mirrors
        And the segments are synchronized
        And check segment conf: postgresql.conf
        And user can start transactions

    Scenario: gpaddmirrors setup recovery part two
        Given the cluster is generated with "3" primaries only
        And all files in gpAdminLogs directory are deleted
        And a gpaddmirrors directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror with content 0 to a new directory with mode 0700
        And edit the input file to add mirror with content 1 to a new directory with mode 0000
        And edit the input file to add mirror with content 2 to a new directory with mode 0000

        When the user runs gpaddmirrors with input file and additional args "-a"
        Then gpaddmirrors should return a return code of 2
        And user can start transactions

        And gpaddmirrors should print "Failed to setup recovery for the following segments" to stdout
        And gpaddmirrors should print "gpaddmirrors error" to stdout
        And gpaddmirrors should not print "Initiating segment recovery" to stdout
        And verify the database has no mirrors

    Scenario Outline: gpaddmirrors can add mirrors even if <failed_count> mirrors failed during basebackup
        Given the cluster is generated with "3" primaries only
        And all files in gpAdminLogs directory are deleted
        And the information of contents 0,1,2 is saved
        And a gpaddmirrors directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror with content <successful_contents> to a new directory with mode 0700
        And edit the input file to add mirror with content <failed_contents> to a new directory with mode 0555

        When the user runs gpaddmirrors with input file and additional args "-a"
        Then gpaddmirrors should return a return code of 1
        And gpaddmirrors should print "Failed to add the following segments" to stdout
        And gpaddmirrors should print "gpaddmirrors failed" to stdout
        And gpaddmirrors should print "Initiating segment recovery" to stdout
        And gpmovemirrors should not print "Segments successfully recovered" to stdout
        Then gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content <successful_contents>
        And gpaddmirrors should print "full" errors to stdout for content <failed_contents>
        And check if mirrors on content 0,1,2 are moved to new location on input file
        And verify there are no recovery backout files

        And verify the database has 3 mirrors
        And user can start transactions

        And verify that mirror on content <successful_contents> is up
        And verify that mirror on content <failed_contents> is down
        And the segments are synchronized for content <successful_contents>

        Given the mode of all the created data directories is changed to 0700
        And the user executes steps required for running in place full recovery for all failed contents
        And verify the database has 3 mirrors
        And all the segments are running
        And the segments are synchronized
        And user can start transactions

    Examples:
        | failed_count | successful_contents | failed_contents |
        | all          | None               | 0,1,2            |
        | some         | 0,1                | 2                |

    Scenario Outline: gpaddmirrors can move even if there are start failures for some of the segments
        Given the cluster is generated with "3" primaries only
        And all files in gpAdminLogs directory are deleted
        And a gprecoverseg directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror with content <successful_contents> to a new directory with mode 0700
        And edit the input file to add mirror with content <failed_contents> to a new directory with mode 0755

        When the user runs gpaddmirrors with input file and additional args "-a"
        Then gpaddmirrors should return a return code of 1
        And gpaddmirrors should print "Initiating segment recovery" to stdout
        And gpaddmirrors should print "Failed to start the following segments" to stdout
        And gpaddmirrors should print "gpaddmirrors failed" to stdout
        And gprecoverseg should not print "Segments successfully recovered" to stdout
        Then gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content <successful_contents>
        And gpaddmirrors should print "start" errors to stdout for content <failed_contents>
        And check if mirrors on content 0,1,2 are moved to new location on input file
        And verify there are no recovery backout files

        And verify the database has 3 mirrors
        And user can start transactions
        And verify that mirror on content <successful_contents> is up
        And verify that mirror on content <failed_contents> is down
        And the segments are synchronized for content <successful_contents>

        And the mode of all the created data directories is changed to 0700
        And the user runs "gprecoverseg -a"
        And gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And user can start transactions
        Examples:
            | failed_count | successful_contents    | failed_contents  |
            | some         | 0,1                    | 2                |
            | all          | None                   | 0,1,2            |


    Scenario: gpaddmirrors can add mirrors and display progress in gpstate
        Given the cluster is generated with "3" primaries only
        And all files in gpAdminLogs directory are deleted
        And sql "DROP TABLE if exists test_add; CREATE TABLE test_add AS SELECT generate_series(1,100000000) AS i" is executed in "postgres" db
        And a gprecoverseg directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror with content 0,1,2 to a new directory with mode 0700

        When the user asynchronously runs gpaddmirrors with input file and additional args "-a" and the process is saved
        And the user suspend the walsender on the primary on content 0
        Then the user waits until recovery_progress.file is created in gpAdminLogs and verifies its format
        And verify that lines from recovery_progress.file are present in segment progress files in gpAdminLogs
        And the user reset the walsender on the primary on content 0
        And the user waits until saved async process is completed
        And recovery_progress.file should not exist in gpAdminLogs in gpAdminLogs
        And the user waits until mirror on content 0,1,2 is up

        And check if mirrors on content 0,1,2 are moved to new location on input file
        And verify there are no recovery backout files

        And verify the database has 3 mirrors
        And user can start transactions
        And verify that mirror on content 0,1,2 is up
        And the segments are synchronized for content 0,1,2

        And all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf
        And all files in gpAdminLogs directory are deleted

    Scenario: gpaddmirrors errors out if the directory for the mirror to be added is not empty
        Given the cluster is generated with "3" primaries only
        And all files in gpAdminLogs directory are deleted
        And a gaddmirrors directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror with content 0,1,2 to a new non-empty directory with mode 0700
        When the user runs gpaddmirrors with input file and additional args "-a"
        Then gpaddmirrors should print "Segment directory '/tmp/.*' exists but is not empty!" to stdout
        And all the segments are running
        And check segment conf: postgresql.conf
        And all files in gpAdminLogs directory are deleted


#    Scenario: gpaddmirrors deletes progress file on SIGINT
#        Given the cluster is generated with "3" primaries only
#        And all files in gpAdminLogs directory are deleted
#        And sql "DROP TABLE if exists test_add; CREATE TABLE test_add AS SELECT generate_series(1,100000000) AS i" is executed in "postgres" db
#        And a gprecoverseg directory under '/tmp' with mode '0700' is created
#        And a gpaddmirrors input file is created
#        And edit the input file to add mirror with content 0,1,2 to a new directory with mode 0700
#
#        When the user asynchronously runs gpaddmirrors with input file and additional args "-a" and the process is saved
#        Then the user asynchronously sets up to end gpaddmirrors process when "Re-running pg_basebackup" is printed in the gpsegrecovery logs
#        And the user waits until recovery_progress.file is created in gpAdminLogs and verifies its format
#        And the user waits until saved async process is completed
#        And recovery_progress.file should not exist in gpAdminLogs
#        And the user waits until mirror on content 0,1,2 is up
#
#        And check if mirrors on content 0,1,2 are moved to new location on input file
#        And verify there are no recovery backout files
#
#        And verify the database has 3 mirrors
#        And user can start transactions
#        And verify that mirror on content 0,1,2 is up
#        And the segments are synchronized for content 0,1,2
#
#        And all the segments are running
#        And the segments are synchronized
#
#        And verify that lines from recovery_progress.file are present in segment progress files in gpAdminLogs
#        And all files in gpAdminLogs directory are deleted

########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: spread mirroring configuration
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with "spread" segment mirroring on "cdw" and "sdw1, sdw2, sdw3"
        Then verify that mirror segments are in "spread" configuration
        Given a preferred primary has failed
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario Outline: gpaddmirrors can add mirrors even if <failed_count> mirrors failed during basebackup
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1, sdw2"
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And a gpaddmirrors directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror on host sdw3 with contents <successful_contents> to a new directory with mode 0700
        And edit the input file to add mirror on host sdw3 with contents <failed_contents> to a new directory with mode 0555

        When the user runs gpaddmirrors with input file and additional args "-a"
        Then gpaddmirrors should return a return code of 1
        And gpaddmirrors should print "Failed to add the following segments" to stdout
        And gpaddmirrors should print "gpaddmirrors failed" to stdout
        And gpaddmirrors should print "Initiating segment recovery" to stdout
        And gpmovemirrors should not print "Segments successfully recovered" to stdout
        Then gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content <successful_contents>
        And gpaddmirrors should print "full" errors to stdout for content <failed_contents>
        And check if mirrors on content 0,1,2,3 are moved to new location on input file
        And verify there are no recovery backout files

        And verify the database has 4 mirrors
        And user can start transactions

        And verify that mirror on content <successful_contents> is up
        And verify that mirror on content <failed_contents> is down
        And the segments are synchronized for content <successful_contents>

        And the mode of all the created data directories is changed to 0700
        And the user executes steps required for running in place full recovery for all failed contents
        And verify the database has 4 mirrors
        And all the segments are running
        And the segments are synchronized
        And user can start transactions
        And the user runs "gpstop -aqM fast"

        Examples:
            | failed_count | successful_contents    | failed_contents  |
            | some         | 0,1,2                  | 3                |
            | all          | None                   | 0,1,2,3          |

    @concourse_cluster
    Scenario Outline: gpaddmirrors can add mirrors even if start fails for <failed_count> mirrors
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1, sdw2"
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And a gpaddmirrors directory under '/tmp' with mode '0700' is created
        And a gpaddmirrors input file is created
        And edit the input file to add mirror on host sdw3 with contents <successful_contents> to a new directory with mode 0700
        And edit the input file to add mirror on host sdw3 with contents <failed_contents> to a new directory with mode 0755

        When the user runs gpaddmirrors with input file and additional args "-a"
        Then gpaddmirrors should return a return code of 1
        And gpaddmirrors should print "Initiating segment recovery" to stdout
        And gpaddmirrors should print "Failed to start the following segments" to stdout
        And gpaddmirrors should print "gpaddmirrors failed" to stdout
        And gpmovemirrors should not print "Segments successfully recovered" to stdout
        Then gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 0,1,2,3
        And gpaddmirrors should print "start" errors to stdout for content <failed_contents>
        And check if mirrors on content 0,1,2,3 are moved to new location on input file
        And verify there are no recovery backout files

        And verify the database has 4 mirrors
        And user can start transactions
        And verify that mirror on content <successful_contents> is up
        And verify that mirror on content <failed_contents> is down
        And the segments are synchronized for content <successful_contents>

        And the mode of all the created data directories is changed to 0700
        And the user runs "gprecoverseg -a"
        And gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And user can start transactions
        And the user runs "gpstop -aqM fast"
        Examples:
            | failed_count | successful_contents    | failed_contents |
            | some         | 0,1,2                  | 3               |
            | all          | None                   | 0,1,2,3         |

    @concourse_cluster
    Scenario: gprecoverseg works correctly on a newly added mirror with HBA_HOSTNAMES=0
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And with HBA_HOSTNAMES "0" a cluster is created with no mirrors on "cdw" and "sdw1, sdw2"
        And pg_hba file "/tmp/gpaddmirrors/data/primary/gpseg0/pg_hba.conf" on host "sdw1" contains only cidr addresses
        And gpaddmirrors adds mirrors
        And pg_hba file "/tmp/gpaddmirrors/data/primary/gpseg0/pg_hba.conf" on host "sdw1" contains only cidr addresses
        And pg_hba file "/tmp/gpaddmirrors/data/primary/gpseg0/pg_hba.conf" on host "sdw1" contains entries for "samehost"
        And verify that the file "pg_hba.conf" in each segment data directory has "no" line starting with "host.*replication.*\(127.0.0\|::1\).*trust"
        Then verify the database has mirrors

        Then the mirror on content 0 is stopped with the immediate flag
        And an FTS probe is triggered
        And the user waits until mirror on content 0 is down
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Initiating segment recovery." to stdout
        Then gprecoverseg should print "skipping pg_rewind on mirror as standby.signal is present" to stdout for mirrors with content 0
        And verify that mirror on content 0 is up
        And all the segments are running
        And the segments are synchronized

        And user immediately stops all primary processes for content 0
        And an FTS probe is triggered
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And check if incremental recovery was successful for mirrors with content 0
        And all the segments are running
        And the segments are synchronized

        When primary and mirror switch to non-preferred roles
        When the user runs "gprecoverseg -a -r"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gprecoverseg works correctly on a newly added mirror with HBA_HOSTNAMES=1
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And with HBA_HOSTNAMES "1" a cluster is created with no mirrors on "cdw" and "sdw1, sdw2"
        And pg_hba file "/tmp/gpaddmirrors/data/primary/gpseg0/pg_hba.conf" on host "sdw1" contains entries for "cdw, sdw1"
        And gpaddmirrors adds mirrors with options "--hba-hostnames"
        And pg_hba file "/tmp/gpaddmirrors/data/primary/gpseg0/pg_hba.conf" on host "sdw1" contains entries for "cdw, sdw1, sdw2, samehost"
        Then verify the database has mirrors

        When the mirror on content 0 is stopped with the immediate flag
        And an FTS probe is triggered
        And the user waits until mirror on content 0 is down
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Initiating segment recovery." to stdout
        Then gprecoverseg should print "skipping pg_rewind on mirror as standby.signal is present" to stdout for mirrors with content 0
        And verify that mirror on content 0 is up
        And all the segments are running
        And the segments are synchronized

        And user immediately stops all primary processes for content 1
        And an FTS probe is triggered
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And check if incremental recovery was successful for mirrors with content 1
        And all the segments are running
        And the segments are synchronized

        When primary and mirror switch to non-preferred roles
        When the user runs "gprecoverseg -a -r"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors puts mirrors on the same hosts when there is a standby configured
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1, sdw2, sdw3"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors
        And save the gparray to context
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1, sdw2, sdw3"
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And gpaddmirrors adds mirrors
        Then mirror hostlist matches the one saved in context
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors puts mirrors on different host
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1, sdw2, sdw3"
        And gpaddmirrors adds mirrors in spread configuration
        Then verify that mirror segments are in "spread" configuration
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors with a default coordinator data directory
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors with a given coordinator data directory [-d <coordinator datadir>]
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1"
        And gpaddmirrors adds mirrors with temporary data dir
        Then verify the database has mirrors
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors mirrors are recognized after a cluster restart
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1"
        When gpaddmirrors adds mirrors
        Then verify the database has mirrors
        When an FTS probe is triggered
        And the user runs "gpstop -a"
        And wait until the process "gpstop" goes down
        And the user runs "gpstart -a"
        And wait until the process "gpstart" goes down
        Then all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors should create consistent port entry on mirrors postgresql.conf file
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1"
        When gpaddmirrors adds mirrors
        Then verify the database has mirrors
        And check segment conf: postgresql.conf
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: gpaddmirrors when the primaries have data
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is not running
        And a cluster is created with no mirrors on "cdw" and "sdw1"
        And database "gptest" exists
        And there is a "heap" table "public.heap_table" in "gptest" with "100" rows
        And there is a "ao" table "public.ao_table" in "gptest" with "100" rows
        And there is a "co" table "public.co_table" in "gptest" with "100" rows
        And gpaddmirrors adds mirrors with temporary data dir
        And an FTS probe is triggered
        And the segments are synchronized
        When user stops all primary processes
        And user can start transactions
        Then verify that there is a "heap" table "public.heap_table" in "gptest" with "202" rows
        Then verify that there is a "ao" table "public.ao_table" in "gptest" with "202" rows
        Then verify that there is a "co" table "public.co_table" in "gptest" with "202" rows
        And the user runs "gpstop -aqM fast"

    @concourse_cluster
    Scenario: tablespaces work on a multi-host environment
        Given a working directory of the test as '/tmp/gpaddmirrors'
          And the database is not running
          And a cluster is created with no mirrors on "cdw" and "sdw1"
          And a tablespace is created with data
         When gpaddmirrors adds mirrors
         Then verify the database has mirrors

         When an FTS probe is triggered
          And the segments are synchronized
         Then the tablespace is valid

         When user stops all primary processes
          And user can start transactions
         Then the tablespace is valid
