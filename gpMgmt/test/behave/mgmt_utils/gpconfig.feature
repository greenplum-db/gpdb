@gpconfig
Feature: gpconfig integration tests

    # Below, we use 2 change (-c) operations to prove that we have changed a
    # file, because any existing postgresql.conf file could already have the
    # first value in it a priori
    #
    # NOTE: since we are SIGHUPing the database after each change (full restarts
    # would be too slow), you must choose parameters with a sighup or weaker
    # context.

    @concourse_cluster
    @demo_cluster
    Scenario Outline: running gpconfig test case: <test_case>, for guc type: <type>
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup
        And the user runs "gpconfig -c <guc> -v <seed_value>"
        And gpconfig should return a return code of 0
        # ensure <guc> is set to <seed_value> for the tests below
        And the user runs "gpstop -u"
        And gpstop should return a return code of 0

       # set same value on coordinator and segments
       When the user runs "gpconfig -c <guc> -v <value>"
       Then gpconfig should return a return code of 0
        And verify that the last line of the file "postgresql.conf" in the coordinator data directory should contain the string "<guc>=<file_value>" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory should contain the string "<guc>=<file_value>"

       # set value on coordinator only, leaving segments the same
       When the user runs "gpconfig -c <guc> -v <value_coordinator_only> --masteronly "
       Then gpconfig should return a return code of 0
        And verify that the last line of the file "postgresql.conf" in the coordinator data directory should contain the string "<guc>=<file_value_coordinator_only>" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory should contain the string "<guc>=<file_value>"

       # set value on coordinator with a different value from the segments
       When the user runs "gpconfig -c <guc> -v <value> -m <value_coordinator>"
       Then gpconfig should return a return code of 0
        And verify that the last line of the file "postgresql.conf" in the coordinator data directory should contain the string "<guc>=<file_value_coordinator>" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory should contain the string "<guc>=<file_value>"

       # now make sure the last changes took full effect as seen by gpconfig
       When the user runs "gpconfig -s <guc> --file"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: <file_value_coordinator>" escaped to stdout
        And gpconfig should print "Segment     value: <file_value>" escaped to stdout

       When the user runs "gpconfig -s <guc> --file-compare"
       Then gpconfig should return a return code of 0
        And gpconfig should print "GUCS ARE OUT OF SYNC" to stdout
        And gpconfig should print "value: <seed_value> | file: <file_value_coordinator>" escaped to stdout
        And gpconfig should print "value: <seed_value> | file: <file_value>" escaped to stdout

       When the user runs "gpstop -u"
       Then gpstop should return a return code of 0

       When the user runs "gpconfig -s <guc> --file-compare"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: <live_value_coordinator> | file: <file_value_coordinator>" escaped to stdout
        And gpconfig should print "Segment     value: <live_value> | file: <file_value>" escaped to stdout

       When the user runs "gpconfig -s <guc>"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: <live_value_coordinator>" escaped to stdout
        And gpconfig should print "Segment     value: <live_value>" escaped to stdout

    Examples:
        | test_case                                   | guc                          | type       | seed_value | value     | file_value | live_value | value_coordinator_only | file_value_coordinator_only | value_coordinator | file_value_coordinator | live_value_coordinator |
        | bool                                        | log_connections              | bool       | off        | on        | on         | on         | off                    | off                         | off               | off                    | off                    |
        | integer                                     | vacuum_cost_limit            | integer    | 300        | 400       | 400        | 400        | 555                    | 555                         | 500               | 500                    | 500                    |
        | integer with memory unit                    | statement_mem                | int w/unit | 123MB      | 500MB     | 500MB      | 500MB      | 500MB                  | 500MB                       | 500MB             | 500MB                  | 500MB                  |
        | integer with time unit                      | statement_timeout            | int w/unit | 1min       | 5min      | 5min       | 5min       | 5min                   | 5min                        | 5min              | 5min                   | 5min                   |
        | float                                       | checkpoint_completion_target | float      | 0.4        | 0.5       | 0.5        | 0.5        | 0.33                   | 0.33                        | 0.7               | 0.7                    | 0.7                    |
        | basic string                                | application_name             | string     | xxxxxx     | bodhi     | 'bodhi'    | bodhi      | lucy                   | 'lucy'                      | bengie            | 'bengie'               | bengie                 |
        | string with spaces                          | application_name             | string     | yyyyyy     | 'bod hi'  | 'bod hi'   | bod hi     | 'bod hi'               | 'bod hi'                    | 'bod hi'          | 'bod hi'               | bod hi                 |
        | different value on coordinator and segments | application_name             | string     | yyyyyy     | 'bod hi'  | 'bod hi'   | bod hi     | 'lu cy'                | 'lu cy'                     | 'ben gie'         | 'ben gie'              | ben gie                |
        | empty string                                | application_name             | string     | zzzzzz     | ''        | ''         |            | ''                     | ''                          | ''                | ''                     |                        |
        | quoted double quotes                        | application_name             | string     | zzzzzz     | '"hi"'    | '"hi"'     | "hi"       | '"hi"'                 | '"hi"'                      | '"hi"'            | '"hi"'                 | "hi"                   |
        | quoted single quotes                        | application_name             | string     | zzzzzz     | "'hi'"    | '''hi'''   | 'hi'       | "'hi'"                 | '''hi'''                    | "'hi'"            | '''hi'''               | 'hi'                   |
        | escaped single quote                        | application_name             | string     | boo        | "\'"      | '\\'''     | \'         | "\'"                   | '\\'''                      | "\'"              | '\\'''                 | \'                     |
        | multiple quoted single quotes               | application_name             | string     | boo        | "''''"    | '''''''''' | ''''       | "''"                   | ''''''                      | "'"               | ''''                   | '                      |
#       | integer with time unit with spaces          | statement_timeout            | int w/unit | 2min       | "'7 min'" | '7 min'    | 7min       | "'7 min'"              | '7 min'                     | "'7 min'"         | '7 min'                | 7min                   |
# 'Integer with time unit with spaces' fails because the live server parses '7 min' as 7min, and our comparison logic does not handle this correctly.
        | utf-8 works                                 | search_path                  | string     | boo        | Ομήρου    | 'Ομήρου'   | Ομήρου     | Ομήρου                 | 'Ομήρου'                    | Ομήρου            | 'Ομήρου'               | Ομήρου                 |
        | client min messages works                   | client_min_messages          | string     | log        | notice    | notice     | notice     | notice                 | notice                      | notice            | notice                 | notice                 |

    @concourse_cluster
    @demo_cluster
    Scenario Outline: write directly to postgresql.conf file: <type>
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup
        # we do this to make sure all segment files contain this <guc>, both in file and live
        And the user runs "gpconfig -c <guc> -v <seed_value>"
        And gpconfig should return a return code of 0
        And the user runs "gpstop -u"
        And gpstop should return a return code of 0

       When the user writes "<guc>" as "<value>" to the coordinator config file
       Then verify that the last line of the file "postgresql.conf" in the coordinator data directory should contain the string "<guc>=<value>" escaped

       # now make sure the last changes took full effect as seen by gpconfig
       When the user runs "gpconfig -s <guc> --file"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: <file_value>" escaped to stdout

       When the user runs "gpstop -u"
       Then gpstop should return a return code of 0

       When the user runs "gpconfig -s <guc>"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: <live_value>" escaped to stdout

    # NOTE: <value> is directly entered into postgresql.conf
    Examples:
        | guc                          | type     | seed_value | value    | file_value | live_value |
        | log_connections              |  bool    | off        | on       | on         | on         |
        | vacuum_cost_limit            |  integer | 300        | 400      | 400        | 400        |
        | checkpoint_completion_target |  real    | 0.4        | 0.5      | 0.5        | 0.5        |
        | application_name             |  string  | xxxxxx     | bodhi    | bodhi      | bodhi      |
        | application_name             |  string  | xxxxxx     | 'bod hi' | 'bod hi'   | bod hi     |
        | application_name             |  string  | xxxxxx     | ''       | ''         |            |

    @concourse_cluster
    @demo_cluster
    Scenario: write a newline using gpconfig
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup
        # we do this to make sure all segment files contain this <guc>, both in file and live
        # todo: we should instead use a custom guc here once we fix the bug that prevents us from setting custom gucs
        And the user runs "gpconfig -c default_text_search_config -v xxxxxx"
        And gpconfig should return a return code of 0
        And the user runs "gpstop -u"
        And gpstop should return a return code of 0

       When the user runs "gpconfig -c default_text_search_config -v $'a\nb'"
       Then verify that the last line of the file "postgresql.conf" in the coordinator data directory should contain the string "default_text_search_config='a\nb'" escaped

       # now make sure the last changes took full effect as seen by gpconfig
       When the user runs "gpconfig -s default_text_search_config --file"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: 'a\nb'" escaped to stdout

       When the user runs "gpstop -u"
       Then gpstop should return a return code of 0

       When the user runs "gpconfig -s default_text_search_config"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Coordinator value: a\nb" to stdout

    @concourse_cluster
    @demo_cluster
    Scenario Outline: gpconfig basic removal for type: <type>
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup
        And the user runs "gpconfig -c <guc> -v <value>"
        And gpconfig should return a return code of 0
        And verify that the file "postgresql.conf" in the coordinator data directory has "some" line starting with "<guc>="
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "<guc>="

       When the user runs "gpconfig -r <guc>"
        And gpconfig should return a return code of 0

       Then verify that the file "postgresql.conf" in the coordinator data directory has "no" line starting with "<guc>="
        And verify that the file "postgresql.conf" in the coordinator data directory has "some" line starting with "#<guc>="
        And verify that the file "postgresql.conf" in each segment data directory has "no" line starting with "<guc>="
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "#<guc>="

    Examples:
        | guc                          | type     | value      |
        | log_connections              |  bool    | off        |
        | vacuum_cost_limit            |  integer | 300        |
        | checkpoint_completion_target |  real    | 0.4        |
        | application_name             |  string  | bengie     |
        | application_name             |  string  | 'ben gie'  |
        | application_name             |  string  | ''         |

    @concourse_cluster
    @demo_cluster
    Scenario Outline: gpconfig when a primary is unreachable
      Given the database is running
        And the user runs "gpconfig -c application_name -v 'initialize'"
        And gpconfig should return a return code of 0
        And the host for the primary on content 0 is made unreachable

       When the user runs "<cmd>"
        And gpconfig should return a return code of <return_code>
       Then gpconfig <should_confirm> print "GUCs on unreachable segment hosts will not be updated. Do you want to continue?" to stdout
        And gpconfig should print "<print_statement>" to stdout

        And verify that the last line of the file "postgresql.conf" in the coordinator data directory <should_update_coordinator> contain the string "application_name='easy'" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory <should_update_segment> contain the string "application_name='easy'"

      Given the cluster is returned to a good state
       When the user runs "gpstop -u"
       Then gpstop should return a return code of 0

        And the user runs "gpconfig -s application_name"
        And gpconfig should return a return code of 0
        And gpconfig <should_update_coordinator> print "Coordinator value: easy" to stdout
        And gpconfig <should_update_segment> print "Segment     value: easy" to stdout

        And the user runs "gpconfig -s application_name --file"
        And gpconfig should return a return code of 0
        And gpconfig <should_update_coordinator> print "Coordinator value: 'easy'" to stdout
        And gpconfig <should_update_segment> print "Segment     value: 'easy'" to stdout

      Examples:
        | test description                                          | return_code | should_confirm | print_statement        | should_update_coordinator | should_update_segment | cmd                                                                               |
        | asks for confirmation and completes when user selects yes | 0           | should         | completed successfully | should                    | should                | gpconfig -c application_name -v "easy" < test/behave/mgmt_utils/steps/data/yes.txt|
        | asks for confirmation and aborts when user selects no     | 1           | should         | User Aborted. Exiting. | should not                | should not            | gpconfig -c application_name -v "easy" < test/behave/mgmt_utils/steps/data/no.txt |
        | does not ask for confirmation for coordinator only change | 0           | should not     | completed successfully | should                    | should not            | gpconfig -c application_name -v "easy" --coordinatoronly                          |

    @concourse_cluster
    @demo_cluster
    Scenario: gpconfig check for GUC's with same prefix
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup

        When the user runs "gpconfig -c gp_resqueue_priority -v 'off'"
        Then gpconfig should return a return code of 0
        And verify that the file "postgresql.conf" in the coordinator data directory has "some" line starting with "gp_resqueue_priority"
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "gp_resqueue_priority"

        When the user runs "gpconfig -c gp_resqueue_priority_cpucores_per_segment -v '4'"
        Then gpconfig should return a return code of 0
        And verify that the file "postgresql.conf" in the coordinator data directory has "some" line starting with "gp_resqueue_priority_cpucores_per_segment"
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "gp_resqueue_priority_cpucores_per_segment"

        When the user runs "gpconfig -c gp_resqueue_priority -v 'on'"
        Then gpconfig should return a return code of 0
        And verify that the file "postgresql.conf" in the coordinator data directory has "some" line starting with "gp_resqueue_priority"
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "gp_resqueue_priority"
        And verify that the file "postgresql.conf" in the coordinator data directory has "some" line starting with "gp_resqueue_priority_cpucores_per_segment"
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "gp_resqueue_priority_cpucores_per_segment"

    @demo_cluster
    Scenario: gpconfig checks liveness of correct number of hosts
      Given the database is running
       When the user runs "gpconfig --debug -c optimizer -v on"
       Then gpconfig should return a return code of 0
        And gpconfig should print "WorkerPool() initialized with 1 workers" escaped to stdout
