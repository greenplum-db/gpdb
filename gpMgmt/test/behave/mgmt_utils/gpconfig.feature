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
    Scenario Outline: run each gpconfig command for guc type: <type>
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup
        And the user runs "gpconfig -c <guc> -v <seed_value>"
        And gpconfig should return a return code of 0
        # ensure <guc> is set to <seed_value> for the tests below
        And the user runs "gpstop -u"
        And gpstop should return a return code of 0

       # set same value on master and segments
       When the user runs "gpconfig -c <guc> -v <value>"
       Then gpconfig should return a return code of 0
        And verify that the last line of the file "postgresql.conf" in the master data directory contains the string "<guc>=<file_value>" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory contains the string "<guc>=<file_value>"

       # set value on master only, leaving segments the same
       When the user runs "gpconfig -c <guc> -v <value_master_only> --masteronly "
       Then gpconfig should return a return code of 0
        And verify that the last line of the file "postgresql.conf" in the master data directory contains the string "<guc>=<file_value_master_only>" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory contains the string "<guc>=<file_value>"

       # set value on master with a different value from the segments
       When the user runs "gpconfig -c <guc> -v <value> -m <value_master>"
       Then gpconfig should return a return code of 0
        And verify that the last line of the file "postgresql.conf" in the master data directory contains the string "<guc>=<file_value_master>" escaped
        And verify that the last line of the file "postgresql.conf" in each segment data directory contains the string "<guc>=<file_value>"

       # now make sure the last changes took full effect as seen by gpconfig
       When the user runs "gpconfig -s <guc> --file"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Master  value: <file_value_master>" escaped to stdout
        And gpconfig should print "Segment value: <file_value>" escaped to stdout

       When the user runs "gpconfig -s <guc> --file-compare"
       Then gpconfig should return a return code of 0
        And gpconfig should print "GUCS ARE OUT OF SYNC" to stdout
        And gpconfig should print "value: <seed_value> | file: <file_value_master>" escaped to stdout
        And gpconfig should print "value: <seed_value> | file: <file_value>" escaped to stdout

       When the user runs "gpstop -u"
       Then gpstop should return a return code of 0

      # FIXME: a file string value of 'my_value' (quotes in file) has a live value of my_value (no quotes) and
      #   --file-compare is currently broken on that case.
#       When the user runs "gpconfig -s <guc> --file-compare"
#       Then gpconfig should return a return code of 0
#        And gpconfig should print "Master[\s]*value: <live_value_master> \| file: <file_value_master>" to stdout
#        And gpconfig should print "Segment[\s]*value: <live_value> \| file: <file_value>" to stdout

       When the user runs "gpconfig -s <guc>"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Master  value: <live_value_master>" escaped to stdout
        And gpconfig should print "Segment value: <live_value>" escaped to stdout

    # test for each type documented for gpconfig
    Examples:
        | guc                          | type     | seed_value | value    | file_value | live_value | value_master_only | file_value_master_only | value_master | file_value_master | live_value_master |
        | log_connections              |  bool    | off        | on       | on         | on         | off               | off                    | off          | off               | off               |
        | gp_resgroup_memory_policy    |  enum    | eager_free | auto     | auto       | auto       | eager_free        | eager_free             | eager_free   | eager_free        | eager_free        |
        | vacuum_cost_limit            |  integer | 300        | 400      | 400        | 400        | 555               | 555                    | 500          | 500               | 500               |
        | checkpoint_completion_target |  real    | 0.4        | 0.5      | 0.5        | 0.5        | 0.33              | 0.33                   | 0.7          | 0.7               | 0.7               |
        | application_name             |  string  | xxxxxx     | bodhi    | 'bodhi'    | bodhi      | lucy              | 'lucy'                 | bengie       | 'bengie'          | bengie            |
        | application_name             |  string  | yyyyyy     | 'bod hi' | 'bod hi'   | bod hi     | 'lu cy'           | 'lu cy'                | 'ben gie'    | 'ben gie'         | ben gie           |
        | application_name             |  string  | zzzzzz     | ''       | ''         |            | ''                | ''                     | ''           | ''                |                   |
        | application_name             |  string  | boo        | "\'"     | '\\'''     | \'         | "\'"              | '\\'''                 | "\'"         | '\\'''            | \'                |
        | application_name             |  string  | boo        | "''''"   | '''''''''' | ''''       | "''"              | ''''''                 | "'"          | ''''              | '                 |
        | search_path                  |  string  | boo        | Ομήρου   | 'Ομήρου'   | Ομήρου     | Ομήρου            | 'Ομήρου'               | Ομήρου       | 'Ομήρου'          | Ομήρου            |

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

       When the user writes "<guc>" as "<value>" to the master config file
       Then verify that the last line of the file "postgresql.conf" in the master data directory contains the string "<guc>=<value>" escaped

       # now make sure the last changes took full effect as seen by gpconfig
       When the user runs "gpconfig -s <guc> --file"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Master  value: <file_value>" escaped to stdout

       When the user runs "gpstop -u"
       Then gpstop should return a return code of 0

       When the user runs "gpconfig -s <guc>"
       Then gpconfig should return a return code of 0
        And gpconfig should print "Master  value: <live_value>" escaped to stdout

    # NOTE: <value> is directly entered into postgresql.conf
    Examples:
        | guc                          | type     | seed_value | value    | file_value | live_value |
        | log_connections              |  bool    | off        | on       | on         | on         |
        | gp_resgroup_memory_policy    |  enum    | eager_free | auto     | auto       | auto       |
        | vacuum_cost_limit            |  integer | 300        | 400      | 400        | 400        |
        | checkpoint_completion_target |  real    | 0.4        | 0.5      | 0.5        | 0.5        |
        | application_name             |  string  | xxxxxx     | bodhi    | bodhi      | bodhi      |
        | application_name             |  string  | xxxxxx     | 'bod hi' | 'bod hi'   | bod hi     |
        | application_name             |  string  | xxxxxx     | ''       | ''         |            |

    @concourse_cluster
    @demo_cluster
    Scenario Outline: gpconfig basic removal for type: <type>
      Given the user runs "gpstop -u"
        And gpstop should return a return code of 0
        And the gpconfig context is setup
        And the user runs "gpconfig -c <guc> -v <value>"
        And gpconfig should return a return code of 0
        And verify that the file "postgresql.conf" in the master data directory has "some" line starting with "<guc>="
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "<guc>="

       When the user runs "gpconfig -r <guc>"
        And gpconfig should return a return code of 0

       Then verify that the file "postgresql.conf" in the master data directory has "no" line starting with "<guc>="
        And verify that the file "postgresql.conf" in the master data directory has "some" line starting with "#<guc>="
        And verify that the file "postgresql.conf" in each segment data directory has "no" line starting with "<guc>="
        And verify that the file "postgresql.conf" in each segment data directory has "some" line starting with "#<guc>="

    Examples:
        | guc                          | type     | value      |
        | log_connections              |  bool    | off        |
        | gp_resgroup_memory_policy    |  enum    | eager_free |
        | vacuum_cost_limit            |  integer | 300        |
        | checkpoint_completion_target |  real    | 0.4        |
        | application_name             |  string  | bengie     |
        | application_name             |  string  | 'ben gie'  |
        | application_name             |  string  | ''         |
