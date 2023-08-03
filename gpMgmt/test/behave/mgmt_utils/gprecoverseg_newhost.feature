@gprecoverseg_newhost
Feature: gprecoverseg tests involving migrating to a new host

########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    # TODO: There is a false dependency on PGDATABASE=gptest in our behave tests, so we create it here.
    @concourse_cluster
    Scenario Outline: "gprecoverseg -p newhosts" successfully recovers for <test_case>
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And database "gptest" exists
      And the user runs gpconfig sets guc "wal_sender_timeout" with "15s"
      And the user runs "gpstop -air"
      And the cluster configuration is saved for "before"
      And saving host IP address of <down>
      And segment hosts <down> are disconnected from the cluster and from the spare segment hosts <spare>
      And the cluster configuration has no segments where <down_sql>
      When the user runs <gprecoverseg_cmd>
      Then gprecoverseg should return a return code of 0
      And pg_hba file "/data/gpdata/mirror/gpseg0/pg_hba.conf" on host "<acting_primary>" contains entries for "<used>"
      And pg_hba file on primary of mirrors on "<used>" with "all" contains no replication entries for <down>
      And verify that only replication connection primary has is to "<used>"
      And the cluster configuration is saved for "<test_case>"
      And the "before" and "<test_case>" cluster configuration matches with the expected for gprecoverseg newhost
      And the mirrors replicate and fail over and back correctly
      And the cluster is rebalanced
      And the original cluster state is recreated for "<test_case>"
      And the cluster configuration is saved for "after_recreation"
      And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost
      Examples:
      | test_case      |  down        | spare | unused | used | acting_primary | gprecoverseg_cmd                              | down_sql                                              |
      | one_host_down  |  "sdw1"      | "sdw5" | "sdw6"   | sdw5 | sdw2           | "gprecoverseg -a -p sdw5 --hba-hostnames"   | "hostname='sdw1' and status='u'"                      |
      | two_hosts_down |  "sdw1,sdw3" | "sdw5,sdw6" | none   | sdw5 | sdw2           | "gprecoverseg -a -p sdw5,sdw6 --hba-hostnames" | "(hostname='sdw1' or hostname='sdw3') and status='u'" |

  @concourse_cluster
  Scenario: "gprecoverseg -p newhost" failure correctly for start failures
    Given the database is running
    And all the segments are running
    And the segments are synchronized
    And database "gptest" exists
    And the user runs gpconfig sets guc "wal_sender_timeout" with "15s"
    And the user runs "gpstop -air"
    And the cluster configuration is saved for "before"
    And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
    And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
    And datadirs from "before" configuration for "sdw1" are created on "sdw5" with mode 755
    When the user runs "gprecoverseg -a -p sdw5 --hba-hostnames"
    Then gprecoverseg should return a return code of 1
#    And pg_hba file "/data/gpdata/mirror/gpseg0/pg_hba.conf" on host "sdw2" contains entries for "sdw5"
    And check if start failed for full recovery for mirrors with hostname sdw5
    And gpAdminLogs directory has no "pg_basebackup*" files on all segment hosts
    And gpAdminLogs directory has no "pg_rewind*" files on all segment hosts
    And gpAdminLogs directory has "gpsegsetuprecovery*" files on all segment hosts
    And gpAdminLogs directory has "gpsegrecovery*" files on all segment hosts
    And datadirs from "before" configuration for "sdw1" are created on "sdw5" with mode 700
    And the user runs "gprecoverseg -aF"
    And gprecoverseg should return a return code of 0
    And check segment conf: postgresql.conf
    And the cluster configuration is saved for "one_host_down"
    And the "before" and "one_host_down" cluster configuration matches with the expected for gprecoverseg newhost
    And the mirrors replicate and fail over and back correctly
    And the cluster is rebalanced
    And the original cluster state is recreated for "one_host_down"
    And the cluster configuration is saved for "after_recreation"
    And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost

  @concourse_cluster
  Scenario: "gprecoverseg -p newhost" failure correctly for basebackup failures
    Given the database is running
    And all the segments are running
    And the segments are synchronized
    And database "gptest" exists
    And the user runs gpconfig sets guc "wal_sender_timeout" with "15s"
    And the user runs "gpstop -air"
    And the cluster configuration is saved for "before"
    And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
    And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
    And the cluster configuration is saved for "before_recoverseg"
    And datadirs from "before_recoverseg" configuration for "sdw1" are created on "sdw5" with mode 000
    When the user runs "gprecoverseg -a -p sdw5 --hba-hostnames"
    Then gprecoverseg should return a return code of 1
#    And pg_hba file "/data/gpdata/mirror/gpseg0/pg_hba.conf" on host "sdw2" contains entries for "sdw5"
    And check if moving the mirrors from sdw1 to sdw5 failed without user termination
    And gprecoverseg should print "Recovery Target instance port        = 20000" to stdout
    And gprecoverseg should print "Recovery Target instance port        = 20001" to stdout
    And gprecoverseg should print "Recovery Target instance port        = 20002" to stdout
    And gprecoverseg should print "Recovery Target instance port        = 20003" to stdout
    And the cluster configuration is saved for "after_backout"
    And the "before_recoverseg" and "after_backout" cluster configuration matches for gprecoverseg newhost
    And datadirs from "before_recoverseg" configuration for "sdw1" are created on "sdw5" with mode 700
    And the user runs "gprecoverseg -a -p sdw5 --hba-hostnames"
    And gprecoverseg should return a return code of 0
    And check segment conf: postgresql.conf
    And the cluster configuration is saved for "one_host_down"
    And the "before" and "one_host_down" cluster configuration matches with the expected for gprecoverseg newhost
    And the mirrors replicate and fail over and back correctly
    And the cluster is rebalanced
    And the original cluster state is recreated for "one_host_down"
    And the cluster configuration is saved for "after_recreation"
    And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost

    @concourse_cluster
    Scenario: failed host is not in reach gprecoverseg recovery works well with all instances recovered
         Given  the database is running
         And all the segments are running
         And the segments are synchronized
         And database "gptest" exists
         And the cluster configuration is saved for "before"
         And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
         And the user runs psql with "-c 'SELECT gp_request_fts_probe_scan()'" against database "postgres"
         And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
         And a gprecoverseg directory under '/tmp' with mode '0700' is created
         And a gprecoverseg input file is created
         And edit the input file to recover with content id 0 to host sdw5
         And edit the input file to recover with content id 1 to host sdw5
         And edit the input file to recover with content id 6 to host sdw5
         And edit the input file to recover with content id 7 to host sdw5
         When the user runs gprecoverseg with input file and additional args "-av"
         Then gprecoverseg should return a return code of 0
         Then the original cluster state is recreated for "one_host_down-1"
         And the cluster configuration is saved for "after_recreation"
         And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost

      @concourse_cluster
      Scenario: failed host is not in reach gprecoverseg recovery works well with partial recovery
         Given  the database is running
         And all the segments are running
         And the segments are synchronized
         And database "gptest" exists
         And the cluster configuration is saved for "before"
         And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
         And the user runs psql with "-c 'SELECT gp_request_fts_probe_scan()'" against database "postgres"
         And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
         And a gprecoverseg directory under '/tmp' with mode '0700' is created
         And a gprecoverseg input file is created
         And edit the input file to recover with content id 0 to host sdw5
         And edit the input file to recover with content id 6 to host sdw5
         When the user runs gprecoverseg with input file and additional args "-av"
         Then gprecoverseg should return a return code of 0
         Then the original cluster state is recreated for "one_host_down-2"
         And the cluster configuration is saved for "after_recreation"
         And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost

      @concourse_cluster
      Scenario: failed host is not in reach gprecoverseg recovery works well only primaries are recovered
         Given  the database is running
         And all the segments are running
         And the segments are synchronized
         And database "gptest" exists
         And the cluster configuration is saved for "before"
         And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
         And the user runs psql with "-c 'SELECT gp_request_fts_probe_scan()'" against database "postgres"
         And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
         And a gprecoverseg directory under '/tmp' with mode '0700' is created
         And a gprecoverseg input file is created
         And edit the input file to recover with content id 0 to host sdw5
         And edit the input file to recover with content id 1 to host sdw5
         When the user runs gprecoverseg with input file and additional args "-av"
         Then gprecoverseg should return a return code of 0
         Then the original cluster state is recreated for "one_host_down-3"
         And the cluster configuration is saved for "after_recreation"
         And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost


  @concourse_cluster
  Scenario: gprecoverseg -p should terminate gracefully on user termination
    Given the database is running
    And all the segments are running
    And the segments are synchronized
    And database "gptest" exists
    And the user runs gpconfig sets guc "wal_sender_timeout" with "15s"
    And the user runs "gpstop -air"
    And the cluster configuration is saved for "before"
    And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
    And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
    And the cluster configuration is saved for "before_recoverseg"
    And datadirs from "before_recoverseg" configuration for "sdw1" are created on "sdw5" with mode 700
    When the user would run "gprecoverseg -a -p sdw5 --hba-hostnames" and terminate the process with SIGTERM
    Then gprecoverseg should return a return code of 1
    And check if moving the mirrors from sdw1 to sdw5 failed with user termination
    And gprecoverseg should print "[WARNING]:-Recieved SIGTERM signal, terminating gprecoverseg" escaped to stdout
    And gprecoverseg should print "[ERROR]:-gprecoverseg process was interrupted by the user." escaped to stdout
    And the cluster configuration is saved for "after_backout"
    And the "before_recoverseg" and "after_backout" cluster configuration matches for gprecoverseg newhost
    When the user runs "gprecoverseg -a -p sdw5 --hba-hostnames"
    Then gprecoverseg should return a return code of 0
    And check segment conf: postgresql.conf
    And the cluster configuration is saved for "one_host_down"
    And the "before" and "one_host_down" cluster configuration matches with the expected for gprecoverseg newhost
    And the mirrors replicate and fail over and back correctly
    And the cluster is rebalanced
    And the original cluster state is recreated for "one_host_down"
    And the cluster configuration is saved for "after_recreation"
    And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost


    @concourse_cluster
    Scenario: gprecoverseg removes the stale replication entries from pg_hba when moving mirrors to new host
      Given the database is running
      And all the segments are running
      And the segments are synchronized
      And the cluster configuration is saved for "before"
      And saving host IP address of "sdw1"
      And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "sdw5"
      And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
      When the user runs "gprecoverseg -a -p sdw5"
      Then gprecoverseg should return a return code of 0
      And all the segments are running
      And pg_hba file on primary of mirrors on "sdw5" with "all" contains no replication entries for "sdw1"
      And verify that only replication connection primary has is to "sdw5"
      And the user runs "gprecoverseg -ar"
      And segment hosts "sdw1" are reconnected to the cluster and to the spare segment hosts "none"
      And segment hosts "sdw5" are disconnected from the cluster and from the spare segment hosts "none"
      And the cluster configuration has no segments where "hostname='sdw5' and status='u'"
      # making the cluster back to it's original state
      And segment hosts "sdw5" are reconnected to the cluster and to the spare segment hosts "none"
      And segment hosts "sdw1" are disconnected from the cluster and from the spare segment hosts "none"
      Then the original cluster state is recreated for "one_host_down"
      And the cluster configuration is saved for "after_recreation"
      And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost

    @concourse_cluster
      Scenario: failover host is not in reach gprecoverseg recovery to new host skips
         Given  the database is running
         And all the segments are running
         And the segments are synchronized
         And database "gptest" exists
         And the cluster configuration is saved for "before"
         And the primary on content 0 is stopped with the immediate flag
         And segment hosts "sdw1,sdw5" are disconnected from the cluster and from the spare segment hosts "sdw6"
         And the user runs psql with "-c 'SELECT gp_request_fts_probe_scan()'" against database "postgres"
         And the cluster configuration has no segments where "hostname='sdw1' and status='u'"
         And a gprecoverseg directory under '/tmp' with mode '0700' is created
         And a gprecoverseg input file is created
         And edit the input file to recover with content id 0 to host sdw5
         When the user runs gprecoverseg with input file and additional args "-av"
         Then gprecoverseg should return a return code of 2
         And gprecoverseg  should print "The recovery target segment sdw5 (content 0) is unreachable" escaped to stdout
