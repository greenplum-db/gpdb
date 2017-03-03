@gpactivatestandby
Feature: gpactivatestandby stop segments
    @wip
    Scenario: Validate gpactivatestandby stop's segments in fast mode
        Given the database is running
        And the database "gpstandbydb" does not exist
        And database "gpstandbydb" exists
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        Given there is a "heap" table "public.test_table" with compression "None" in "gpstandbydb" with data
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        And the numbers "1" to "1000000" are inserted into "public.test_table" tables in "gpstandbydb"
        When the user runs the query "select count(*) from public.test_table a, public.test_table b, public.test_table c, public.test_table d, public.test_table e" on "gpstandbydb"
        And the user runs "gpstop -a -m -M fast" 
        Then gpstop should return a return code of 0
        When the performance timer is started
        And the user activates standby on the standbyhost
        Then gpactivatestandby should return a return code of 0
        And the performance timer should be less then "300" seconds
        When the user initializes standby master on "mdw"
        Then gpinitstandby should return a return code of 0
        And the user runs the command "gpstop -a -m -M fast" from standby
        And gpstop should return a return code of 0
        When the user runs "gpactivatestandby -d $MASTER_DATA_DIRECTORY -fa"
        Then gpactivatestandby should return a return code of 0
        And database "gpstandbydb" health check should pass on table "public.test_table"
        And the standby is initialized if required


    @wip
    @dca
    Scenario: Sanity test gpstart with standby activated
        Given the database is running
        And all the segments are running
        and the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        When the user runs "gpstop -a"
        Then gpstop should return a return code of 0
        When the user runs "gpstart -a"
        Then gpstart should return a return code of 0
        And all the segments are running
        When the user runs "gpinitstandby -r -a"
        Then gpinitstandby should return a return code of 0

    @wip
    @dca
    Scenario: Pid corresponds to a non postgres process 
        Given the database is running
        and all the segments are running
        and the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        When the postmaster.pid file on "smdw" segment is saved
        And the user runs "gpstop -a" 
        And gpstop should return a return code of 0
        And the background pid is killed on "smdw" segment
        When we run a sample background script to generate a pid on "smdw" segment
        And we generate the postmaster.pid file with the background pid on "smdw" segment
        Then the user runs "gpstart -a" 
        And gpstart should return a return code of 0
        And all the segments are running
        and the segments are synchronized
        And the backup pid file is deleted on "smdw" segment
        And the background pid is killed on "smdw" segment
        And the user runs "gpinitstandby -r -a " 
        And gpinitstandby should return a return code of 0

    @wip
    @dca
    Scenario: Pid does not correspond to any running process
        Given the database is running
        And all the segments are running
        and the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        When the postmaster.pid file on "smdw" segment is saved
        And the user runs "gpstop -a" 
        And gpstop should return a return code of 0
        And we generate the postmaster.pid file with a non running pid on the same "smdw" segment
        Then the user runs "gpstart -a" 
        And gpstart should return a return code of 0
        And all the segments are running
        and the segments are synchronized
        And the backup pid file is deleted on "smdw" segment
        And the user runs "gpinitstandby -r -a " 
        And gpinitstandby should return a return code of 0

    @wip
    @dca
    Scenario: Pid corresponds to a postmaster process 
        Given the database is running
        and all the segments are running
        and the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        And the user runs "gpstop -am" 
        And gpstop should return a return code of 0
        And the user activates standby on the standbyhost
        Then gpactivatestandby should return a return code of 0
        Then the user runs "gpstart -a"
        And gpstart should return a return code of 2
        Then the user runs "gpstop -a"
        And gpstart should return a return code of 2
        When the user initializes standby master on "mdw"
        And the user runs the command "gpstop -am" from standby 
        And the user runs "gpactivatestandby -a"
        Then gpactivatestandby should return a return code of 0
        When the user initializes standby master on "smdw"

    Scenario: Syncmaster is running on the smdw with no PID file
        Given the database is running
        and all the segments are running
        and the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        And save the cluster configuration
        And the user runs "gpstop -a"
        And the user stops the syncmaster
        When the user starts the syncmaster
        And the user runs the command "rm $MASTER_DATA_DIRECTORY/postmaster.pid" from standby
        And the user runs the command "rm /tmp/.s.PGSQL.$PGPORT*" from standby
        And the user runs "gpstart -a"
        Then gpstart should return a return code of 1
        And gpstart should print Standby Master could not be started to stdout
        And the user stops the syncmaster

    @wip
    Scenario: gpactivatestandby should not ssh to localhost
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        When the user runs "gpstop -ma"
        And user can "not" ssh locally on standby
        And the user activates standby on the standbyhost
        Then gpactivatestandby should return a return code of 0
        And user can " " ssh locally on standby
        When the user initializes standby master on "mdw"
        Then gpinitstandby should return a return code of 0
        And the user runs the command "gpstop -a -m -M fast" from standby
        And gpstop should return a return code of 0
        When the user runs "gpactivatestandby -d $MASTER_DATA_DIRECTORY -fa"
        Then gpactivatestandby should return a return code of 0
