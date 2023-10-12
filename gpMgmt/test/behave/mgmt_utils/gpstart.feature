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
          And the catalog has a standby master entry

         When the standby host is made unreachable
          And the user runs command "pkill -9 postgres"
          And "gpstart" is run with prompts accepted

         Then gpstart should print "Continue only if you are certain that the standby is not acting as the master." to stdout
          And gpstart should print "No standby master configured" to stdout
          And gpstart should return a return code of 0
          And all the segments are running

    @demo_cluster
    Scenario: gpstart runs with given master data directory option
        Given the database is running
          And running postgres processes are saved in context
          And the user runs "gpstop -a"
          And gpstop should return a return code of 0
          And verify no postgres process is running on all hosts
          And "MASTER_DATA_DIRECTORY" environment variable is not set
         Then the user runs utility "gpstart" with master data directory and "-a"
          And gpstart should return a return code of 0
          And "MASTER_DATA_DIRECTORY" environment variable should be restored
          And all the segments are running

    @demo_cluster
    Scenario: gpstart priorities given master data directory over env option
        Given the database is running
          And running postgres processes are saved in context
          And the user runs "gpstop -a"
          And gpstop should return a return code of 0
          And verify no postgres process is running on all hosts
          And the environment variable "MASTER_DATA_DIRECTORY" is set to "/tmp/"
         Then the user runs utility "gpstart" with master data directory and "-a"
          And gpstart should return a return code of 0
          And "MASTER_DATA_DIRECTORY" environment variable should be restored
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

    @concourse_cluster
    @demo_cluster
    Scenario: non-super user 'foouser' can connect to psql database
        Given the database is running
          And the user runs psql with "-c 'create user foouser login;'" against database "postgres"
          And the user runs command "echo 'local all foouser trust' >> $MASTER_DATA_DIRECTORY/pg_hba.conf"

         When the user runs psql with "-c '\l'" against database "postgres"
         Then psql should return a return code of 0

    @concourse_cluster
    @demo_cluster
    Scenario Outline: "gpstart" accepts <test_scenarios> when utility mode is set to <utility_mode>
        Given the database is not running
          And the user runs "gpstart -a"
          And "gpstart -a" should return a return code of 0

         When The user runs psql "<psql_cmd>" against database "postgres" when utility mode is set to "<utility_mode>"
         Then psql_cmd should return a return code of 0

          And the user runs "gpstop -ai"
          And "gpstop -ai" should return a return code of 0

     Examples:
         | test_scenarios             | utility_mode            | psql_cmd             |
         | super user connections     | True                    | -c '\l'              |
         | non-super user connections | True                    | -U foouser -c '\l'   |
         | super user connections     | False                   | -c '\l'              |
         | non-super user connections | False                   | -U foouser -c '\l'   |

    @concourse_cluster
    @demo_cluster
    Scenario Outline: "gpstart -m" <database> <test_scenarios> when utility mode is set to <utility_mode>
        Given the database is not running
          And the user runs "gpstart -ma"
          And "gpstart -ma" should return a return code of 0

         When The user runs psql "<psql_cmd>" against database "postgres" when utility mode is set to <utility_mode>
         Then psql_cmd should return a return code of <return_code>
          And psql_cmd "<error_out_state>" print "<error_msg>" error message

          And the user runs "gpstop -mai"
          And "gpstop -mai" should return a return code of 0

     Examples:
         | test_scenarios             | utility_mode            | psql_cmd             | return_code | database      | error_out_state | error_msg                                                                                                       |
         | super user connections     | True                    | -c '\l'              | 0           | accepts       | should not      | psql: FATAL:  System was started in master-only utility mode - only utility mode connections are allowed        |
         | non-super user connections | True                    | -U foouser -c '\l'   | 0           | accepts       | should not      | psql: FATAL:  System was started in master-only utility mode - only utility mode connections are allowed        |
         | super user connections     | False                   | -c '\l'              | 2           | rejects       | should          | psql: FATAL:  System was started in master-only utility mode - only utility mode connections are allowed        |
         | non-super user connections | False                   | -U foouser -c '\l'   | 2           | rejects       | should          | psql: FATAL:  System was started in master-only utility mode - only utility mode connections are allowed        |

    @concourse_cluster
    @demo_cluster
    Scenario Outline: "gpstart -m -R" <database> <test_scenarios> when utility mode is set to <utility_mode>
        Given the database is not running
          And the user runs "gpstart -mRa"
          And "gpstart -mRa" should return a return code of 0

         When The user runs psql "<psql_cmd>" against database "postgres" when utility mode is set to <utility_mode>
         Then psql_cmd should return a return code of <return_code>
          And psql_cmd "<error_out_state>" print "<error_msg>" error message

          And the user runs "gpstop -mai"
          And "gpstop -mai" should return a return code of 0

      Examples:
         | test_scenarios             | utility_mode            | psql_cmd             | return_code | database      | error_out_state | error_msg                                                                                                       |
         | super user connections     | True                    | -c '\l'              | 0           | accepts       | should not      | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |
         | non-super user connections | True                    | -U foouser -c '\l'   | 2           | rejects       | should          | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |
         | super user connections     | False                   | -c '\l'              | 2           | rejects       | should          | psql: FATAL:  System was started in master-only utility mode - only utility mode connections are allowed        |
         | non-super user connections | False                   | -U foouser -c '\l'   | 2           | rejects       | should          | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |

    @concourse_cluster
    @demo_cluster
    Scenario Outline: "gpstart -R" <database> <test_scenarios> when utility mode is set to <utility_mode>
        Given the database is not running
          And the user runs "gpstart -Ra"
          And "gpstart -Ra" should return a return code of 0

         When The user runs psql "<psql_cmd>" against database "postgres" when utility mode is set to <utility_mode>
         Then psql_cmd should return a return code of <return_code>
          And psql_cmd "<error_out_state>" print "<error_msg>" error message

          And the user runs "gpstop -ai"
          And "gpstop -ai" should return a return code of 0

       Examples:
         | test_scenarios             | utility_mode            | psql_cmd             | return_code | database      | error_out_state | error_msg                                                                                                       |
         | super user connections     | True                    | -c '\l'              | 0           | accepts       | should not      | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |
         | non-super user connections | True                    | -U foouser -c '\l'   | 2           | rejects       | should          | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |
         | super user connections     | False                   | -c '\l'              | 0           | accepts       | should not      | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |
         | non-super user connections | False                   | -U foouser -c '\l'   | 2           | rejects       | should          | psql: error: FATAL:  remaining connection slots are reserved for non-replication superuser connections          |

    @concourse_cluster
    @demo_cluster
    Scenario: Removal of non-super user role succeeds
        Given the database is not running
          And the user runs "gpstart -a"
          And "gpstart -a" should return a return code of 0

          When the user runs psql with "-c 'drop user foouser;'" against database "postgres"
          Then psql should return a return code of 0
