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

    Scenario Outline: full recovery limits number of parallel processes correctly
        Given a standard local demo cluster is created
        And 2 gprecoverseg directory under '/tmp/recoverseg' with mode '0700' is created
        And a good gprecoverseg input file is created for moving 2 mirrors
        When the user runs gprecoverseg with input file and additional args "-a -F -v <args>"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should only spawn up to <coordinator_workers> workers in WorkerPool
        And check if gprecoverseg ran "$GPHOME/sbin/gpsegsetuprecovery.py" 1 times with args "-b <segHost_workers>"
        And check if gprecoverseg ran "$GPHOME/sbin/gpsegrecovery.py" 1 times with args "-b <segHost_workers>"
        And gpsegsetuprecovery should only spawn up to <segHost_workers> workers in WorkerPool
        And gpsegrecovery should only spawn up to <segHost_workers> workers in WorkerPool
        And check if gprecoverseg ran "$GPHOME/sbin/gpsegstop.py" 1 times with args "-b <segHost_workers>"
        And the segments are synchronized

      Examples:
        | args      | coordinator_workers | segHost_workers |
        | -B 1 -b 1 |  1                  |  1              |
        | -B 2 -b 1 |  2                  |  1              |
        | -B 1 -b 2 |  1                  |  2              |

    Scenario Outline: Rebalance correctly limits the number of concurrent processes
      Given the database is running
      And user stops all primary processes
      And user can start transactions
      And the user runs "gprecoverseg -a -v <args>"
      And gprecoverseg should return a return code of 0
      And the segments are synchronized
      When the user runs "gprecoverseg -ra -v <args>"
      Then gprecoverseg should return a return code of 0
      And gprecoverseg should only spawn up to <coordinator_workers> workers in WorkerPool
      And gpsegsetuprecovery should only spawn up to <segHost_workers> workers in WorkerPool
      And gpsegrecovery should only spawn up to <segHost_workers> workers in WorkerPool
      And check if gprecoverseg ran "$GPHOME/sbin/gpsegsetuprecovery.py" 1 times with args "-b <segHost_workers>"
      And check if gprecoverseg ran "$GPHOME/sbin/gpsegrecovery.py" 1 times with args "-b <segHost_workers>"
      And check if gprecoverseg ran "$GPHOME/sbin/gpsegstop.py" 1 times with args "-b <segHost_workers>"
      And the segments are synchronized

    Examples:
      | args      | coordinator_workers | segHost_workers |
      | -B 1 -b 1 |  1                  |  1              |
      | -B 2 -b 1 |  2                  |  1              |
      | -B 1 -b 2 |  1                  |  2              |

    Scenario: gprecoverseg should not output bootstrap error on success
        Given the database is running
        And user immediately stops all primary processes
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Running recovery and starting the required segments" to stdout
        And gprecoverseg should print "pg_rewind: Done!" to stdout for each mirror
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
        And the segments are synchronized
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should not print "Unhandled exception in thread started by <bound method Worker.__bootstrap" to stdout
	And the segments are synchronized

    Scenario: gprecoverseg full recovery displays pg_basebackup progress to the user
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And user stops all mirror processes
        When user can start transactions
        And the user runs "gprecoverseg -F -a -s"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for each mirror
        And gpAdminLogs directory has no "pg_basebackup*" files
        And gpAdminLogs directory has "gpsegrecovery*" files
        And gpAdminLogs directory has "gpsegsetuprecovery*" files
        And all the segments are running
        And the segments are synchronized

    Scenario: gprecoverseg mixed recovery displays pg_basebackup and rewind progress to the user
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted
      And user immediately stops all primary processes
      And user can start transactions
      And sql "DROP TABLE if exists test_mixed_recovery; CREATE TABLE test_mixed_recovery AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_mixed_recovery" table row count in "postgres" is saved
      And a gprecoverseg directory under '/tmp' with mode '0700' is created
      And a gprecoverseg input file is created
      And edit the input file to recover mirror with content 0 to a new directory with mode 0700
      And edit the input file to recover mirror with content 1 full inplace
      And edit the input file to recover mirror with content 2 incremental
      When the user runs gprecoverseg with input file and additional args "-av"
      Then gprecoverseg should return a return code of 0
      And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 0,1
      And gprecoverseg should print "pg_rewind: Done!" to stdout for mirrors with content 2
      And check if gprecoverseg ran gpsegsetuprecovery.py 1 times with the expected args
      And check if gprecoverseg ran gpsegrecovery.py 1 times with the expected args
      And gpAdminLogs directory has no "pg_basebackup*" files
      And gpAdminLogs directory has no "pg_rewind*" files
      And gpAdminLogs directory has "gpsegsetuprecovery*" files
      And gpAdminLogs directory has "gpsegrecovery*" files
      And all the segments are running
      And the segments are synchronized
      And the user runs "gprecoverseg -ar"
      And gprecoverseg should return a return code of 0
      And the row count from table "test_mixed_recovery" in "postgres" is verified against the saved data

  Scenario: gprecoverseg mixed recovery one basebackup fails and one rewind fails while others succeed
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted
      And user immediately stops all primary processes
      And user can start transactions
      And sql "DROP TABLE if exists test_rewind_failure; CREATE TABLE test_rewind_failure AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_rewind_failure" table row count in "postgres" is saved
      And all files in pg_wal directory are deleted from datadirectory of content 2 mirror
      And a gprecoverseg directory under '/tmp' with mode '0700' is created
      And a gprecoverseg input file is created
    # TODO: add test for moving a mirror to a good directory, which updates the catalog
      And edit the input file to recover mirror with content 0 to a new directory with mode 0000
      And edit the input file to recover mirror with content 1 full inplace
      And edit the input file to recover mirror with content 2 incremental
      When the user runs gprecoverseg with input file and additional args "-a"
      Then gprecoverseg should return a return code of 2
      And gprecoverseg should print "pg_basebackup: error: could not access directory" to stdout for mirrors with content 0
      And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 1
      And gprecoverseg should print "pg_rewind: fatal" to stdout for mirrors with content 2
      # TODO: this should only be 1 file ( for the failed segment)
      And gpAdminLogs directory has "pg_basebackup*" files only for content 0,1
      And gpAdminLogs directory has "pg_rewind*" files only for content 2
      And gpAdminLogs directory has "gpsegsetuprecovery*" files
      And gpAdminLogs directory has "gpsegrecovery*" files
      And an FTS probe is triggered
      And the mirror for content 1 are up
      And the segments are synchronized for content 1
      And the mode of the saved data directory is changed to 700
      And the user runs "gprecoverseg -aF && gprecoverseg -ar"
      And all the segments are running
      And the segments are synchronized
      And gprecoverseg should return a return code of 0
      And the row count from table "test_rewind_failure" in "postgres" is verified against the saved data

  Scenario: gprecoverseg mixed recovery segments come up even if one basebackup takes longer
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted
      And user immediately stops all primary processes
      And user can start transactions
      And the user suspend the walsender on the primary on content 0
      And sql "DROP TABLE if exists test_slow_basebackup; CREATE TABLE test_slow_basebackup AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_slow_basebackup" table row count in "postgres" is saved
      And a gprecoverseg directory under '/tmp' with mode '0700' is created
      And a gprecoverseg input file is created
      And edit the input file to recover mirror with content 0 full inplace
      And edit the input file to recover mirror with content 1 full inplace
      And edit the input file to recover mirror with content 2 incremental
      When the user asynchronously runs gprecoverseg with input file and additional args "-a" and the process is saved
      Then the user waits until mirror on content 1 is up
      And the user waits until mirror on content 2 is up
      And verify that mirror on content 0 is down
      And the user reset the walsender on the primary on content 0
      And the user waits until saved async process is completed
      And gpAdminLogs directory has no "pg_basebackup*" files
      And gpAdminLogs directory has no "pg_rewind*" files
      And gpAdminLogs directory has "gpsegsetuprecovery*" files
      And gpAdminLogs directory has "gpsegrecovery*" files
      And all the segments are running
      And the segments are synchronized
      And the user runs "gprecoverseg -ar"
      And gprecoverseg should return a return code of 0
      And the row count from table "test_slow_basebackup" in "postgres" is verified against the saved data

  Scenario: gprecoverseg incremental recovery segments come up even if one rewind fails
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted
      And user immediately stops all primary processes
      And user can start transactions
      And sql "DROP TABLE if exists test_rewind_failure; CREATE TABLE test_rewind_failure AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_rewind_failure" table row count in "postgres" is saved
      And all files in pg_wal directory are deleted from datadirectory of content 0 mirror
      When the user runs "gprecoverseg -a"
      Then gprecoverseg should return a return code of 2
      And gprecoverseg should print "pg_rewind: fatal" to stdout for mirrors with content 0
      And gprecoverseg should print "pg_rewind: Done!" to stdout for mirrors with content 1,2
      # TODO: this should only be 1 file ( for the failed segment)
      And gpAdminLogs directory has "pg_rewind*" files only for content 0,1,2
      And gpAdminLogs directory has "gpsegsetuprecovery*" files
      And gpAdminLogs directory has "gpsegrecovery*" files
      And an FTS probe is triggered
      And the mirror for content 1,2 are up
      And the segments are synchronized for content 1,2
      And the user runs "gprecoverseg -aF && gprecoverseg -ar"
      And gprecoverseg should return a return code of 0
      And all the segments are running
      And the segments are synchronized
      And the row count from table "test_rewind_failure" in "postgres" is verified against the saved data

  Scenario: gprecoverseg mixed recovery segments come up even if one pg_ctl_start fails
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted
      And user immediately stops all primary processes
      And user can start transactions
      And sql "DROP TABLE if exists test_start_failure; CREATE TABLE test_start_failure AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_start_failure" table row count in "postgres" is saved
      And a gprecoverseg directory under '/tmp' with mode '0700' is created
      And a gprecoverseg input file is created
      And edit the input file to recover mirror with content 0 to a new directory with mode 0755
      And edit the input file to recover mirror with content 1 full inplace
      And edit the input file to recover mirror with content 2 incremental
      When the user runs gprecoverseg with input file and additional args "-a"
      Then gprecoverseg should return a return code of 2
      And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 0,1
      And gprecoverseg should print "pg_rewind: Done!" to stdout for mirrors with content 2
      # TODO: this should only be 1 file ( for the failed segment)
      And gpAdminLogs directory has "pg_basebackup*" files only for content 0,1
      And gpAdminLogs directory has "pg_rewind*" files only for content 2
      And gpAdminLogs directory has "gpsegsetuprecovery*" files
      And gpAdminLogs directory has "gpsegrecovery*" files
      And an FTS probe is triggered
      And verify that mirror on content 0 is down
      And the mirror for content 1,2 are up
      And the segments are synchronized for content 1,2
      And the mode of the saved data directory is changed to 700
      And the user runs "gprecoverseg -aF && gprecoverseg -ar"
      And all the segments are running
      And the segments are synchronized
      And gprecoverseg should return a return code of 0
      And the row count from table "test_start_failure" in "postgres" is verified against the saved data

  Scenario: gprecoverseg incremental recovery displays pg_rewind progress to the user
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And all files in gpAdminLogs directory are deleted
        And user immediately stops all primary processes
        And user can start transactions
        When the user runs "gprecoverseg -a -s"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "pg_rewind: Done!" to stdout for each mirror
        And gpAdminLogs directory has no "pg_rewind*" files
      And gpAdminLogs directory has "gpsegsetuprecovery*" files
      And gpAdminLogs directory has "gpsegrecovery*" files
        And all the segments are running
        And the segments are synchronized
        And the cluster is rebalanced

    Scenario: gprecoverseg does not display pg_basebackup progress to the user when --no-progress option is specified
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And user stops all mirror processes
        When user can start transactions
        And the user runs "gprecoverseg -F -a --no-progress"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Running recovery and starting the required segments" to stdout
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
        And we run a sample background script to generate a pid on "primary" segment
        And we generate the postmaster.pid file with the background pid on "primary" segment
        And the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Running recovery and starting the required segments" to stdout
        And gprecoverseg should print "pg_rewind: no rewind required" to stdout for each mirror
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
        And gprecoverseg should print "Running recovery and starting the required segments" to stdout
        And gprecoverseg should print "pg_rewind: no rewind required" to stdout for each mirror
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

    Scenario: gprecoverseg incremental recovery displays status for mirrors after pg_rewind call
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And user stops all mirror processes
        And user can start transactions
        When the user runs "gprecoverseg -a -s"
        And gprecoverseg should print "skipping pg_rewind on mirror as standby.signal is present" to stdout
        Then gprecoverseg should return a return code of 0
        And gpAdminLogs directory has no "pg_rewind*" files
        And all the segments are running
        And the segments are synchronized
        And the cluster is rebalanced

    @backup_restore_bashrc
    Scenario: gprecoverseg should not return error when banner configured on host
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        When the user sets banner on host
        And user stops all mirror processes
        And user can start transactions
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And the cluster is rebalanced

    @demo_cluster
    @concourse_cluster
    Scenario Outline: <scenario> recovery skips unreachable segments
      Given the database is running
      And all the segments are running
      And the segments are synchronized

      And the primary on content 0 is stopped
      And user can start transactions
      And the primary on content 1 is stopped
      And user can start transactions
      And the status of the primary on content 0 should be "d"
      And the status of the primary on content 1 should be "d"

      And the host for the primary on content 1 is made unreachable

      And the user runs psql with "-c 'CREATE TABLE IF NOT EXISTS foo (i int)'" against database "postgres"
      And the user runs psql with "-c 'INSERT INTO foo SELECT generate_series(1, 10000)'" against database "postgres"

      When the user runs "gprecoverseg <args>"
      Then gprecoverseg should print "One or more hosts are not reachable via SSH." to stdout
      And gprecoverseg should print "Host invalid_host is unreachable" to stdout
      And the user runs psql with "-c 'SELECT gp_request_fts_probe_scan()'" against database "postgres"
      And the status of the primary on content 0 should be "u"
      And the status of the primary on content 1 should be "d"

      # Rebalance all possible segments and skip unreachable segment pairs.
      When the user runs "gprecoverseg -ar"
      Then gprecoverseg should return a return code of 0
      And gprecoverseg should print "Not rebalancing primary segment dbid \d with its mirror dbid \d because one is either down, unreachable, or not synchronized" to stdout
      And content 0 is balanced
      And content 1 is unbalanced

      And the user runs psql with "-c 'DROP TABLE foo'" against database "postgres"
      And the cluster is returned to a good state

      Examples:
        | scenario    | args |
        | incremental | -a   |
        | full        | -aF  |

########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: gprecoverseg incremental recovery segments come up even if one rewind fails
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted on hosts mdw,sdw1,sdw2
      And the "primary" segment information is saved
      And user immediately stops all primary processes
      And user can start transactions
      And sql "DROP TABLE if exists test_rewind_failure; CREATE TABLE test_rewind_failure AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_rewind_failure" table row count in "postgres" is saved
      And all files in pg_wal directory are deleted from data directory of saved primary
      When the user runs "gprecoverseg -a"
      Then gprecoverseg should return a return code of 2
      And gprecoverseg should print "pg_rewind: fatal" to stdout for mirrors with content 0
      And gprecoverseg should print "pg_rewind: Done!" to stdout for mirrors with content 1,2,3
          # TODO: this should only be 1 file ( for the failed segment)
      And gpAdminLogs directory has "pg_rewind*" files on respective hosts only for content 0,1,2,3
#      And gpAdminLogs directory has "gpsegsetuprecovery*" files
#      And gpAdminLogs directory has "gpsegrecovery*" files
      And an FTS probe is triggered
      And the mirror for content 1,2,3 are up
      And the segments are synchronized for content 1,2,3
      And the user runs "gprecoverseg -aF && gprecoverseg -ar"
      And gprecoverseg should return a return code of 0
      And all the segments are running
      And the segments are synchronized
      And the row count from table "test_rewind_failure" in "postgres" is verified against the saved data

    @concourse_cluster
    Scenario: gprecoverseg mixed recovery segments come up even if one basebackup takes longer
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted on hosts mdw,sdw1,sdw2
      And user immediately stops all primary processes
      And user can start transactions
      And the user suspend the walsender on the primary on content 0
      And sql "DROP TABLE if exists test_slow_basebackup; CREATE TABLE test_slow_basebackup AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_slow_basebackup" table row count in "postgres" is saved
      And a gprecoverseg directory under '/tmp' with mode '0700' is created
      And a gprecoverseg input file is created
      And edit the input file to recover mirror with content 0 full inplace
      And edit the input file to recover mirror with content 1 incremental
      And edit the input file to recover mirror with content 2 full inplace
      And edit the input file to recover mirror with content 3 incremental
      When the user asynchronously runs gprecoverseg with input file and additional args "-a" and the process is saved
      Then the user waits until mirror on content 1 is up
      And the user waits until mirror on content 2 is up
      And the user waits until mirror on content 3 is up
      And verify that mirror on content 0 is down
      And the user reset the walsender on the primary on content 0
      And the user waits until saved async process is completed
#      And gpAdminLogs directory has no "pg_basebackup*" files
#      And gpAdminLogs directory has no "pg_rewind*" files
#      And gpAdminLogs directory has "gpsegsetuprecovery*" files
#      And gpAdminLogs directory has "gpsegrecovery*" files
      And all the segments are running
      And the segments are synchronized
      And the user runs "gprecoverseg -ar"
      And gprecoverseg should return a return code of 0
      And the row count from table "test_slow_basebackup" in "postgres" is verified against the saved data

    @concourse_cluster
    Scenario: gprecoverseg mixed recovery one basebackup fails and one rewind fails while others succeed
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And all files in gpAdminLogs directory are deleted on hosts mdw,sdw1,sdw2
      And the "primary" segment information is saved
      And user immediately stops all primary processes
      And user can start transactions
      And sql "DROP TABLE if exists test_rewind_failure; CREATE TABLE test_rewind_failure AS SELECT generate_series(1,10000) AS i" is executed in "postgres" db
      And the "test_rewind_failure" table row count in "postgres" is saved
      And all files in pg_wal directory are deleted from data directory of saved primary
      And a gprecoverseg directory under '/tmp' with mode '0700' is created
      And a gprecoverseg input file is created
      # TODO: add test for moving a mirror to a good directory, which updates the catalog
      And edit the input file to recover mirror with content 0 incremental
      And edit the input file to recover mirror with content 1 full inplace
      And edit the input file to recover mirror with content 2 incremental
      And edit the input file to recover mirror with content 3 to a new directory on remote host with mode 0000
      When the user runs gprecoverseg with input file and additional args "-a"
      Then gprecoverseg should return a return code of 2
      And gprecoverseg should print "pg_rewind: fatal" to stdout for mirrors with content 0
      And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content 1
      And gprecoverseg should print "pg_rewind: Done!" to stdout for mirrors with content 2
      And gprecoverseg should print "pg_basebackup: error: could not access directory" to stdout for mirrors with content 3
        # TODO: this should only be 1 file ( for the failed segment)
      And gpAdminLogs directory has "pg_basebackup*" files on respective hosts only for content 1,3
      And gpAdminLogs directory has "pg_rewind*" files on respective hosts only for content 0,2
#      And gpAdminLogs directory has "gpsegsetuprecovery*" files
#      And gpAdminLogs directory has "gpsegrecovery*" files
      And an FTS probe is triggered
      And the mirror for content 1,2 are up
      And the segments are synchronized for content 1,2
      And the mode of all the created data directories is changed to 0700
      And the user runs "gprecoverseg -aF && gprecoverseg -ar"
      And gprecoverseg should return a return code of 0
      And all the segments are running
      And the segments are synchronized
      And gprecoverseg should return a return code of 0
      And the row count from table "test_rewind_failure" in "postgres" is verified against the saved data

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
        And gprecoverseg should print "Running recovery and starting the required segments" to stdout
        And gprecoverseg should print "pg_basebackup: base backup completed" to stdout
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
        And gprecoverseg should print "Heap checksum setting is consistent between coordinator and the segments that are candidates for recoverseg" to stdout
        When the user runs "gprecoverseg -ra"
        Then gprecoverseg should return a return code of 0
        And gprecoverseg should print "Heap checksum setting is consistent between coordinator and the segments that are candidates for recoverseg" to stdout
        And all the segments are running
        And the segments are synchronized
        # validate the new segment has the correct setting by getting admin connection to that segment
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

  @concourse_cluster
  Scenario: moving mirror to a different host must work
      Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the information of a "mirror" segment on a remote host is saved
        And the information of the corresponding primary segment on a remote host is saved
       When user kills a "mirror" process with the saved information
        And user can start transactions
       Then the saved "mirror" segment is marked down in config
       When the user runs "gprecoverseg -a -p mdw"
       Then gprecoverseg should return a return code of 0
       When user kills a "primary" process with the saved information
        And user can start transactions
       Then the saved "primary" segment is marked down in config
       When the user runs "gprecoverseg -a"
       Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
       When the user runs "gprecoverseg -ra"
       Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized

  @concourse_cluster
  Scenario: recovering a host with tablespaces succeeds
      Given the database is running

        # Add data including tablespaces
        And a tablespace is created with data
        And database "gptest" exists
        And the user connects to "gptest" with named connection "default"
        And the user runs psql with "-c 'CREATE TABLE public.before_host_is_down (i int) DISTRIBUTED BY (i)'" against database "gptest"
        And the user runs psql with "-c 'INSERT INTO public.before_host_is_down SELECT generate_series(1, 10000)'" against database "gptest"
        And the "public.before_host_is_down" table row count in "gptest" is saved

        # Stop one of the nodes as if for hardware replacement and remove any traces as if it was a new node.
        # Recoverseg requires the host being restored have the same hostname.
        And the user runs "gpstop -a --host sdw1"
        And gpstop should return a return code of 0
        And the user runs remote command "rm -rf /data/gpdata/*" on host "sdw1"
        And user can start transactions

        # Add data after one of the nodes is down for maintenance
        And database "gptest" exists
        And the user connects to "gptest" with named connection "default"
        And the user runs psql with "-c 'CREATE TABLE public.after_host_is_down (i int) DISTRIBUTED BY (i)'" against database "gptest"
        And the user runs psql with "-c 'INSERT INTO public.after_host_is_down SELECT generate_series(1, 10000)'" against database "gptest"
        And the "public.after_host_is_down" table row count in "gptest" is saved

        # restore the down node onto a node with the same hostname
        When the user runs "gprecoverseg -a -p sdw1"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And user can start transactions
        And the user runs "gprecoverseg -ra"
        And gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And user can start transactions

        # verify the data
        And the tablespace is valid
        And the row count from table "public.before_host_is_down" in "gptest" is verified against the saved data
        And the row count from table "public.after_host_is_down" in "gptest" is verified against the saved data
