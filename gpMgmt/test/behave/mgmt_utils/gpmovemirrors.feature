@gpmovemirrors
Feature: Tests for gpmovemirrors

    Scenario: gpmovemirrors fails with totally malformed input file
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'malformed' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 3

    Scenario: gpmovemirrors fails with bad host in input file
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'badhost' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 3

    Scenario: gpmovemirrors fails with invalid option parameter
        Given a standard local demo cluster is running
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'good' gpmovemirrors file is created
        When the user runs gpmovemirrors with additional args "--invalid-option"
        Then gpmovemirrors should return a return code of 2

    Scenario: gpmovemirrors can change the location of mirrors within a single host
        Given a standard local demo cluster is created
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'good' gpmovemirrors file is created
        And verify replication slot internal_wal_replication_slot is available on all the segments
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 0
        And verify the database has mirrors
        #gpmovemirrors triggers full recovery where old replication slot is dropped and new one is created
        And verify replication slot internal_wal_replication_slot is available on all the segments
        And all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf
        And verify that mirrors are recognized after a restart

    Scenario: gpmovemirrors can change the port of mirrors within a single host
        Given a standard local demo cluster is created
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'samedir' gpmovemirrors file is created
        And verify replication slot internal_wal_replication_slot is available on all the segments
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 0
        And verify the database has mirrors
        #gpmovemirrors triggers full recovery where old replication slot is dropped and new one is created
        And verify replication slot internal_wal_replication_slot is available on all the segments
        And all the segments are running
        And the segments are synchronized
        And verify that mirrors are recognized after a restart
        And check segment conf: postgresql.conf

    Scenario: gpmovemirrors gives a warning when passed identical attributes for new and old mirrors
        Given a standard local demo cluster is created
        And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a 'identicalAttributes' gpmovemirrors file is created
        When the user runs gpmovemirrors
        Then gpmovemirrors should return a return code of 0
	And gpmovemirrors should print a "request to move a mirror with identical attributes" warning
	And verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirrors are recognized after a restart

    Scenario: tablespaces work
        Given a standard local demo cluster is created
          And a tablespace is created with data
          And a gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
          And a 'good' gpmovemirrors file is created
         When the user runs gpmovemirrors
         Then gpmovemirrors should return a return code of 0
          And verify the database has mirrors
          And all the segments are running
          And the segments are synchronized
          And verify that mirrors are recognized after a restart
          And the tablespace is valid

    Scenario Outline: gpmovemirrors limits number of parallel processes correctly
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And a tablespace is created with data
        And 2 gpmovemirrors directory under '/tmp/gpmovemirrors' with mode '0700' is created
        And a good gpmovemirrors file is created for moving 2 mirrors
        When the user runs gpmovemirrors with additional args "<args>"
        Then gpmovemirrors should return a return code of 0
        And check if gpmovemirrors ran "$GPHOME/bin/gprecoverseg" 1 times with args "<args>"
        And gpmovemirrors should only spawn up to <coordinator_workers> workers in WorkerPool
        And verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirrors are recognized after a restart
        And the tablespace is valid

    Examples:
        | args         | coordinator_workers |
        | -B 1 -b 1 -v |  1                  |
        | -B 2 -b 1 -v |  2                  |
        | -B 1 -b 2 -v |  1                  |

        """
        gpmovemirrors test cases
        all but one mirror fails to move - assert all others moved. also test with and without running backout
            fix error and then run the failed mirror again
        all mirrors fail to move - test with and without running backout
            fix error and then run the failed mirror again
        not just for movemirrors:
            add a validation error like both hosts recoverying to the same port - so that the triplet code fails
                assert that gp_seg_config wasn't updated
        """

    Scenario Outline: user can <correction> if <failed_count> mirrors failed to move initially
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And the information of contents 0,1,2 is saved
        #TODO tablespace tests were failing intermittently why ?
        And a tablespace is created with data
        And a gpmovemirrors directory under '/tmp' with mode '0700' is created
        And a gpmovemirrors input file is created
        And edit the input file to move mirror with content <successful_contents> to a new directory with mode 0700
        And edit the input file to move mirror with content <failed_contents> to a new directory with mode 0000

        When the user runs gpmovemirrors with input file and additional args " "
        Then gpmovemirrors should return a return code of 3
        And user can start transactions

        And gpmovemirrors should print "Initiating segment recovery" to stdout
        And gpmovemirrors should print "Failed to recover the following segments" to stdout
        And gpmovemirrors should print "full" errors to stdout for content <failed_contents>
        And gpmovemirrors should print "gprecoverseg failed. Please check the output" to stdout
        And verify that mirror on content <successful_contents> is up
        And verify that mirror on content <failed_contents> is down
        And verify there are no recovery backout files
        And check if mirrors on content <failed_contents> are in their original configuration
        And check if mirrors on content <successful_contents> are moved to new location on input file
        And verify there are no recovery backout files

        And the tablespace is valid
        Then the contents <failed_contents> should have their original data directory in the system configuration
        And the gp_configuration_history table should contain a backout entry for the mirror segment for contents <failed_contents>

        And the user executes steps required for <correction_steps>
        And all the segments are running
        And the segments are synchronized
        And user can start transactions
    Examples:
        | correction                | failed_count | successful_contents | failed_contents | correction_steps                                     |
        | rerun gpmovemirrors       | all          | None               | 0,1,2          | rerunning gpmovemirrors for contents 0,1,2             |
        | rerun gpmovemirrors       | some         | 0,1                | 2              | rerunning gpmovemirrors for contents 2                 |
        | run gprecoverseg          | some         | 0                  | 1,2            | running in place full recovery for all failed contents |
        | run gprecoverseg          | all          | None               | 0,1,2          | running in place full recovery for all failed contents |

    @skip_cleanup
    Scenario: gpmovemirrors can move mirrors even if start fails for some mirrors
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And a gpmovemirrors directory under '/tmp' with mode '0700' is created
        And a gpmovemirrors input file is created
        And edit the input file to move mirror with content 0 to a new directory with mode 0700
        And edit the input file to move mirror with content 1 to a new directory with mode 0700
        And edit the input file to move mirror with content 2 to a new directory with mode 0755

        When the user runs gpmovemirrors with input file and additional args " "
        Then gpmovemirrors should return a return code of 3
        And user can start transactions

        And gpmovemirrors should print "Initiating segment recovery" to stdout
        And gpmovemirrors should not print "Segments successfully recovered" to stdout
        And gpaddmirrors should print "Failed to start the following segments" to stdout
        And gpmovemirrors should print "gprecoverseg failed" to stdout
        And gpmovemirrors should print "start" errors to stdout for content 2
        And verify that mirror on content 0,1 is up
        And verify that mirror on content 2 is down
        Then gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 0,1,2
        And check if mirrors on content 0,1,2 are moved to new location on input file


        Given the mode of all the created data directories is changed to 0700
        When the user runs "gprecoverseg -a"
        And gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf
        And user can start transactions


  @demo_cluster
  Scenario: gpmovemirrors -i creates recovery_progress.file if some mirrors are moved
    Given the database is running
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster
    And user can start transactions
    And sql "DROP TABLE if exists test_movemirrors; CREATE TABLE test_movemirrors AS SELECT generate_series(1,100000000) AS i" is executed in "postgres" db
    And a gpmovemirrors directory under '/tmp' with mode '0700' is created
    And a gpmovemirrors input file is created
    And edit the input file to recover mirror with content 0 to a new directory on remote host with mode 0700
    And edit the input file to recover mirror with content 1 to a new directory on remote host with mode 0700
    When the user asynchronously runs gpmovemirrors with input file and additional args " " and the process is saved
    And the user waits until mirror on content 0,1 is down
    And the user suspend the walsender on the primary on content 0
    Then the user waits until recovery_progress.file is created in gpAdminLogs and verifies its format
    And verify that lines from recovery_progress.file are present in segment progress files in gpAdminLogs
    And the user reset the walsender on the primary on content 0
    And the user waits until saved async process is completed
    And recovery_progress.file should not exist in gpAdminLogs
    And the user waits until mirror on content 0,1 is up
    And check if mirrors on content 0,1 are moved to new location on input file
    And user can start transactions
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster

  @demo_cluster
  Scenario: gpmovemirrors -i creates recovery_progress.file if all mirrors are moved
    Given the database is running
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster
    And user can start transactions
    And sql "DROP TABLE if exists test_movemirrors; CREATE TABLE test_movemirrors AS SELECT generate_series(1,100000000) AS i" is executed in "postgres" db
    And a gpmovemirrors directory under '/tmp' with mode '0700' is created
    And a gpmovemirrors input file is created
    And edit the input file to recover mirror with content 0 to a new directory on remote host with mode 0700
    And edit the input file to recover mirror with content 1 to a new directory on remote host with mode 0700
    And edit the input file to recover mirror with content 2 to a new directory on remote host with mode 0700
    When the user asynchronously runs gpmovemirrors with input file and additional args " " and the process is saved
    And the user waits until mirror on content 0,1,2 is down
    And the user suspend the walsender on the primary on content 0
    Then the user waits until recovery_progress.file is created in gpAdminLogs and verifies its format
    And verify that lines from recovery_progress.file are present in segment progress files in gpAdminLogs
    And the user reset the walsender on the primary on content 0
    And the user waits until saved async process is completed
    And recovery_progress.file should not exist in gpAdminLogs
    And the user waits until mirror on content 0,1,2 is up
    And check if mirrors on content 0,1,2 are moved to new location on input file
    And user can start transactions
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster
    And the cluster is recovered in full and rebalanced

  @demo_cluster
  @concourse_cluster
  @skip_cleanup
  Scenario: gpmovemirrors gives warning if pg_basebackup is already running for one of the mirrors to be moved
    Given the database is running
    And all the segments are running
    And the segments are synchronized
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster
    And the information of contents 0,1,2 is saved
    And user immediately stops all mirror processes for content 0,1,2
    And user can start transactions
    And the user suspend the walsender on the primary on content 0
    And the user asynchronously runs "gprecoverseg -aF" and the process is saved
    And the user just waits until recovery_progress.file is created in gpAdminLogs
    And user waits until gp_stat_replication table has no pg_basebackup entries for content 1,2
    And an FTS probe is triggered
    And the user waits until mirror on content 1,2 is up
    And verify that mirror on content 0 is down
    And the gprecoverseg lock directory is removed
    And user immediately stops all mirror processes for content 1,2
    And the user waits until mirror on content 1,2 is down
    And a gpmovemirrors directory under '/tmp' with mode '0700' is created
    And a gpmovemirrors input file is created
    And edit the input file to recover mirror with content 0,1,2 to a new directory with mode 0700
    When the user runs gpmovemirrors with input file and additional args " "
    Then gprecoverseg should print "Found pg_basebackup running for segments with contentIds [0], skipping recovery of these segments" to logfile
    And gprecoverseg should return a return code of 0
    And gpmovemirrors should return a return code of 0
    And verify that mirror on content 1,2 is up
    And verify that mirror on content 0 is down
    And check if mirrors on content 1,2 are moved to new location on input file
    And check if mirrors on content 0 are in their original configuration
    And an FTS probe is triggered
    And the user reset the walsender on the primary on content 0
    And the user waits until saved async process is completed
    And recovery_progress.file should not exist in gpAdminLogs
    And verify that mirror on content 0 is up
    And the cluster is recovered in full and rebalanced
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster

  @demo_cluster
  @concourse_cluster
  @skip_cleanup
  Scenario: gpmovemirrors gives warning if pg_basebackup is already running for some of the mirrors to be moved
    Given the database is running
    And all the segments are running
    And the segments are synchronized
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster
    And the information of contents 0,1,2 is saved
    And user immediately stops all mirror processes for content 0,1,2
    And user can start transactions
    And the user suspend the walsender on the primary on content 0
    And the user suspend the walsender on the primary on content 1
    And the user asynchronously runs "gprecoverseg -aF" and the process is saved
    And the user just waits until recovery_progress.file is created in gpAdminLogs
    And user waits until gp_stat_replication table has no pg_basebackup entries for content 2
    And an FTS probe is triggered
    And the user waits until mirror on content 2 is up
    And verify that mirror on content 0,1 is down
    And the gprecoverseg lock directory is removed
    And user immediately stops all mirror processes for content 2
    And the user waits until mirror on content 2 is down
    And a gpmovemirrors directory under '/tmp' with mode '0700' is created
    And a gpmovemirrors input file is created
    And edit the input file to recover mirror with content 0,1,2 to a new directory with mode 0700
    When the user runs gpmovemirrors with input file and additional args " "
    Then gprecoverseg should print "Found pg_basebackup running for segments with contentIds [0, 1], skipping recovery of these segments" to logfile
    And gprecoverseg should return a return code of 0
    And gpmovemirrors should return a return code of 0
    And verify that mirror on content 2 is up
    And verify that mirror on content 0,1 is down
    And check if mirrors on content 2 are moved to new location on input file
    And check if mirrors on content 0,1 are in their original configuration
    And an FTS probe is triggered
    And the user reset the walsender on the primary on content 0
    And the user reset the walsender on the primary on content 1
    And the user waits until saved async process is completed
    And recovery_progress.file should not exist in gpAdminLogs
    And verify that mirror on content 0,1 is up
    And the cluster is recovered in full and rebalanced
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster

  @demo_cluster
  @concourse_cluster
  @skip_cleanup
  Scenario: gpmovemirrors gives warning if pg_basebackup is already running for all mirrors to be moved
    Given the database is running
    And all the segments are running
    And the segments are synchronized
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster
    And the information of contents 0,1,2 is saved
    And user immediately stops all mirror processes for content 0,1,2
    And user can start transactions
    And the user suspend the walsender on the primary on content 0
    And the user suspend the walsender on the primary on content 1
    And the user suspend the walsender on the primary on content 2
    And the user asynchronously runs "gprecoverseg -aF" and the process is saved
    And the user just waits until recovery_progress.file is created in gpAdminLogs
    And verify that mirror on content 0,1,2 is down
    And the gprecoverseg lock directory is removed
    Given a gpmovemirrors directory under '/tmp' with mode '0700' is created
    And a gpmovemirrors input file is created
    And edit the input file to recover mirror with content 0,1,2 to a new directory with mode 0700
    When the user runs gpmovemirrors with input file and additional args "-v"
    Then gprecoverseg should print "Found pg_basebackup running for segments with contentIds [0, 1, 2], skipping recovery of these segments" to logfile
    And gprecoverseg should return a return code of 0
    And gpmovemirrors should return a return code of 0
    And check if mirrors on content 0,1,2 are in their original configuration
    And an FTS probe is triggered
    And the user reset the walsender on the primary on content 0
    And the user reset the walsender on the primary on content 1
    And the user reset the walsender on the primary on content 2
    And the user waits until saved async process is completed
    And recovery_progress.file should not exist in gpAdminLogs
    And verify that mirror on content 0,1,2 is up
    And the cluster is recovered in full and rebalanced
    And all files in gpAdminLogs directory are deleted on all hosts in the cluster


########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: gpmovemirrors can change from group mirroring to spread mirroring
        Given verify that mirror segments are in "group" configuration
        And pg_hba file "/data/gpdata/primary/gpseg1/pg_hba.conf" on host "sdw1" contains only cidr addresses
        And a sample gpmovemirrors input file is created in "spread" configuration
        When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_spread"
        Then gpmovemirrors should return a return code of 0
        # Verify that mirrors are functional in the new configuration
        Then verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        And verify that mirror segments are in "spread" configuration
        And verify that mirrors are recognized after a restart
        And pg_hba file "/data/gpdata/primary/gpseg1/pg_hba.conf" on host "sdw1" contains only cidr addresses
        And the information of a "mirror" segment on a remote host is saved
        When user kills a "mirror" process with the saved information
        And an FTS probe is triggered
        And user can start transactions
        Then the saved "mirror" segment is marked down in config
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And the information of the corresponding primary segment on a remote host is saved
        When user kills a "primary" process with the saved information
        And an FTS probe is triggered
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        When primary and mirror switch to non-preferred roles
        When the user runs "gprecoverseg -a -r"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf

    @concourse_cluster
    Scenario: gpmovemirrors can change from spread mirroring to group mirroring
        Given verify that mirror segments are in "spread" configuration
        And a sample gpmovemirrors input file is created in "group" configuration
        When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_group --hba-hostnames"
        Then gpmovemirrors should return a return code of 0
        # Verify that mirrors are functional in the new configuration
        Then verify the database has mirrors
        And all the segments are running
        And the segments are synchronized
        # gpmovemirrors_input_group moves mirror on sdw3 to sdw2, corresponding primary should now have sdw2 entry
        And pg_hba file "/data/gpdata/primary/gpseg1/pg_hba.conf" on host "sdw1" contains entries for "sdw2"
        And verify that mirror segments are in "group" configuration
        And verify that mirrors are recognized after a restart
        And the information of a "mirror" segment on a remote host is saved
        When user kills a "mirror" process with the saved information
        And an FTS probe is triggered
        And user can start transactions
        Then the saved "mirror" segment is marked down in config
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And the information of the corresponding primary segment on a remote host is saved
        When user kills a "primary" process with the saved information
        And an FTS probe is triggered
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        When primary and mirror switch to non-preferred roles
        When the user runs "gprecoverseg -a -r"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And check segment conf: postgresql.conf

    @concourse_cluster
    Scenario: tablespaces work on a multi-host environment
        Given verify that mirror segments are in "group" configuration
          And a tablespace is created with data
          And a sample gpmovemirrors input file is created in "spread" configuration
         When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_spread"
         Then gpmovemirrors should return a return code of 0
          And verify the tablespace directories on host "sdw2" for content "1" are deleted
          And verify the tablespace directories on host "sdw1" for content "5" are deleted
          And verify the tablespace directories on host "sdw3" for content "1" are valid
          And verify the tablespace directories on host "sdw2" for content "5" are valid
          And verify the database has mirrors
          And all the segments are running
          And the segments are synchronized
          And verify that mirrors are recognized after a restart
          And the tablespace is valid

         When user stops all primary processes
          And user can start transactions
         Then the tablespace is valid
          And the cluster is recovered in full and rebalanced

    @concourse_cluster
    Scenario: gpmovemirrors mirrors come up even if one pg_ctl_start fails
        Given the database is running
        And verify that mirror segments are in "spread" configuration
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And the information of contents 0,1,2 is saved

        And sql "DROP TABLE if exists test_movemirrors; CREATE TABLE test_movemirrors AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
        And the "test_movemirrors" table row count in "postgres" is saved

        And a gpmovemirrors directory under '/tmp' with mode '0700' is created
        And a gpmovemirrors input file is created
        And edit the input file to recover mirror with content 0 to a new directory on remote host with mode 0755
        And edit the input file to recover mirror with content 1 to a new directory on remote host with mode 0700
        And edit the input file to recover mirror with content 2 to a new directory on remote host with mode 0700

        When the user runs gpmovemirrors
        Then check if start failed for contents 0 during full recovery for gpmovemirrors
        And check if full recovery was successful for mirrors with content 1,2
        And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 0,1,2
        And gprecoverseg should print "Initiating segment recovery." to stdout

        And check if mirrors on content 0,1,2 are moved to new location on input file
        And gpAdminLogs directory has no "pg_basebackup*" files on all segment hosts
        And gpAdminLogs directory has no "pg_rewind*" files on all segment hosts
        And gpAdminLogs directory has "gpsegsetuprecovery*" files on all segment hosts
        And gpAdminLogs directory has "gpsegrecovery*" files on all segment hosts

        And the mode of all the created data directories is changed to 0700
        And the cluster is recovered in full and rebalanced
        And check segment conf: postgresql.conf
        And the row count from table "test_movemirrors" in "postgres" is verified against the saved data

    @concourse_cluster
    Scenario: gpmovemirrors mirrors come up even if one basebackup fails
        Given the database is running
        And verify that mirror segments are in "spread" configuration
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And the information of contents 0,1,2 is saved
        And check segment conf: postgresql.conf

        And sql "DROP TABLE if exists test_movemirrors; CREATE TABLE test_movemirrors AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
        And the "test_movemirrors" table row count in "postgres" is saved

        And a gpmovemirrors directory under '/tmp' with mode '0700' is created
        And a gpmovemirrors input file is created
        And edit the input file to recover mirror with content 0 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 1 to a new directory on remote host with mode 0700
        And edit the input file to recover mirror with content 2 to a new directory on remote host with mode 0700

        When the user runs gpmovemirrors
        Then check if full recovery failed for mirrors with content 0 for gpmovemirrors
        And check if full recovery was successful for mirrors with content 1,2
        And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 1,2
        And check if mirrors on content 0 are in their original configuration
        And check if mirrors on content 1,2 are moved to new location on input file
        And verify that mirror on content 1,2,3,4,5 is up
        And gpAdminLogs directory has "pg_basebackup*" files on respective hosts only for content 0
        And gpAdminLogs directory has no "pg_rewind*" files on all segment hosts
        And gpAdminLogs directory has "gpsegsetuprecovery*" files on all segment hosts
        And gpAdminLogs directory has "gpsegrecovery*" files on all segment hosts
        And check segment conf: postgresql.conf

        And the mode of all the created data directories is changed to 0700
        And the cluster is recovered in full and rebalanced
        And check segment conf: postgresql.conf
        And the row count from table "test_movemirrors" in "postgres" is verified against the saved data

    @concourse_cluster
    Scenario: gpmovemirrors mirrors works even if all the mirrors to moved fail during basebackup
        Given the database is running
        And verify that mirror segments are in "spread" configuration
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And the information of contents 0,1,2,3,4,5 is saved
        And check segment conf: postgresql.conf

        And sql "DROP TABLE if exists test_movemirrors; CREATE TABLE test_movemirrors AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
        And the "test_movemirrors" table row count in "postgres" is saved

        And a gpmovemirrors directory under '/tmp' with mode '0700' is created
        And a gpmovemirrors input file is created
        And edit the input file to recover mirror with content 0 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 1 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 2 to a new directory on remote host with mode 0000

        When the user runs gpmovemirrors
        Then check if full recovery failed for mirrors with content 0,1,2 for gpmovemirrors
        And verify that mirror on content 3,4,5 is up

        And check if mirrors on content 0,1,2,3,4,5 are in their original configuration

        And gpAdminLogs directory has "pg_basebackup*" files on respective hosts only for content 0,1,2
        And gpAdminLogs directory has no "pg_rewind*" files on all segment hosts
        And gpAdminLogs directory has "gpsegsetuprecovery*" files on all segment hosts
        And gpAdminLogs directory has "gpsegrecovery*" files on all segment hosts
        And check segment conf: postgresql.conf

        And the mode of all the created data directories is changed to 0700
        And the cluster is recovered in full and rebalanced
        And check segment conf: postgresql.conf
        And the row count from table "test_movemirrors" in "postgres" is verified against the saved data

    @concourse_cluster
    Scenario: gpmovemirrors mirrors works even if all mirrors are moved and all fail during basebackup
        Given the database is running
        And verify that mirror segments are in "spread" configuration
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted on all hosts in the cluster
        And the information of contents 0,1,2,3,4,5 is saved
        And check segment conf: postgresql.conf

        And sql "DROP TABLE if exists test_movemirrors; CREATE TABLE test_movemirrors AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
        And the "test_movemirrors" table row count in "postgres" is saved

        And a gpmovemirrors directory under '/tmp' with mode '0700' is created
        And a gpmovemirrors input file is created
        And edit the input file to recover mirror with content 0 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 1 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 2 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 3 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 4 to a new directory on remote host with mode 0000
        And edit the input file to recover mirror with content 5 to a new directory on remote host with mode 0000

        When the user runs gpmovemirrors
        Then check if full recovery failed for mirrors with content 0,1,2,3,4,5 for gpmovemirrors
        And check if mirrors on content 0,1,2,3,4,5 are in their original configuration

        And verify there are no recovery backout files
        And gpAdminLogs directory has "pg_basebackup*" files on respective hosts only for content 0,1,2,3,4,5
        And gpAdminLogs directory has no "pg_rewind*" files on all segment hosts
        And gpAdminLogs directory has "gpsegsetuprecovery*" files on all segment hosts
        And gpAdminLogs directory has "gpsegrecovery*" files on all segment hosts
        And check segment conf: postgresql.conf

        And the mode of all the created data directories is changed to 0700
        And the cluster is recovered in full and rebalanced
        And check segment conf: postgresql.conf
        And the row count from table "test_movemirrors" in "postgres" is verified against the saved data
