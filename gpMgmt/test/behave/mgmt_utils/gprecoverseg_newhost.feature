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
      And segment hosts <down> are disconnected from the cluster and from the spare segment hosts <spare>
      And the cluster configuration has no segments where <down_sql>
      When the user runs <gprecoverseg_cmd>
      Then gprecoverseg should return a return code of 0
      And the cluster configuration is saved for "<test_case>"
      And the "before" and "<test_case>" cluster configuration matches with the expected for gprecoverseg newhost
      And gprecoverseg should print "Skipping shared memory cleanup and gpsegstop on unreachable host" to stdout
      And gprecoverseg should not print "ExecutionError" to stdout
      And the mirrors replicate and fail over and back correctly
      And segment hosts <down> are reconnected to the cluster and to the spare segment hosts "<unused>"
      And the original cluster state is recreated after cleaning up <down> hosts
      And database "gptest" exists
      And the cluster configuration is saved for "after_recreation"
      And the "before" and "after_recreation" cluster configuration matches with the expected for gprecoverseg newhost
      Examples:
      | test_case      |  down        | spare       | unused | gprecoverseg_cmd               | down_sql                                              |
      | one_host_down  |  "sdw1"      | "sdw5,sdw6" | sdw6   | "gprecoverseg -a -p sdw5"      | "hostname='sdw1' and status='u'"                      |
      | two_hosts_down |  "sdw1,sdw3" | "sdw5,sdw6" | none   | "gprecoverseg -a -p sdw5,sdw6" | "(hostname='sdw1' or hostname='sdw3') and status='u'" |
