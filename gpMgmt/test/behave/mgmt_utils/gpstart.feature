@gpstart
Feature: gpstart behave tests

    @concourse_cluster
    @demo_cluster
    Scenario: gpstart correctly identifies down segments
        Given the database is running
          And a mirror has crashed
          And the database is not running
         When the user runs "gpstart -a"
         Then gpstart should return a return code of 0
          And gpstart should print "Skipping startup of segment marked down in configuration" to stdout
          And gpstart should print "Skipped segment starts \(segments are marked down in configuration\) += 1" to stdout
          And gpstart should print "Successfully started [0-9]+ of [0-9]+ segment instances, skipped 1 other segments" to stdout
          And gpstart should print "Number of segments not attempted to start: 1" to stdout

    Scenario: gpstart starts even if the standby host is unreachable
        Given the database is running
          And the catalog has a standby coordinator entry

         When the standby host is made unreachable
          And the user runs command "pkill -9 postgres"
          And "gpstart" is run with prompts accepted

         Then gpstart should print "Continue only if you are certain that the standby is not acting as the coordinator." to stdout
          And gpstart should print "No standby coordinator configured" to stdout
          And gpstart should return a return code of 0
          And all the segments are running

    @concourse_cluster
    @demo_cluster
    Scenario: gpstart starts even if a segment host is unreachable
        Given the database is running
          And the host for the primary on content 0 is made unreachable
          And the host for the mirror on content 1 is made unreachable

          And the user runs command "pkill -9 postgres" on all hosts without validation
         When "gpstart" is run with prompts accepted

         Then gpstart should print "Host invalid_host is unreachable" to stdout
          And gpstart should print unreachable host messages for the down segments
          And the status of the primary on content 0 should be "d"
          And the status of the mirror on content 1 should be "d"

          And the cluster is returned to a good state
