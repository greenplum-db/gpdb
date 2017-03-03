@gpstart
Feature: Validate command line arguments

    Scenario: Entering an invalid argument
        When the user runs "gpstart -z"
        Then gpstart should print no such option: -z error message 
        And gpstart should return a return code of 2

    Scenario: Run gpstart
        Given the database is not running
        When the user runs "gpstart -a"
        Then gpstart should return a return code of 0
        And database "testdb" health check should pass on table "t1"

    @dca
    Scenario: gpstart with 1 host down
        Given we have determined the first segment hostname
        And the database is not running
        And eth2 on the first segment host is down
        And eth3 on the first segment host is down
        When the user runs "gpstart -a"
        Then gpstart should return a return code of 1
        And database "testdb" health check should pass on table "t1"
        And eth2 on the first segment host is up
        And eth3 on the first segment host is up
        And the user runs "gprecoverseg -a"
        And the segments are synchronized
        And the user runs "gprecoverseg -r"
        And the segments are synchronized

    Scenario: Remove MASTER_DATA_DIRECTORY from os.environ
        Given the database is not running
        And MASTER_DATA_DIRECTORY environment variable is not set
        When the user runs "gpstart -a"
        Then gpstart should return a return code of 2
        And gpstart should print Environment Variable MASTER_DATA_DIRECTORY not set! to stdout
        And MASTER_DATA_DIRECTORY environment variable should be restored
        
    
    # gpstart fails after hard shutdown of the system, the postmaster.pid file exists with a pid
    # that matches that of a currently running non-postgres pid

    @dca
    Scenario: Pid corresponds to a non postgres process 
        Given the database is running
        and all the segments are running
        and the segments are synchronized
        and the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And the user runs "gpstop -a"
        And gpstop should return a return code of 0
        And the background pid is killed on "primary" segment
        When we run a sample background script to generate a pid on "primary" segment
        And we generate the postmaster.pid file with the background pid on "primary" segment
        Then the user runs "gpstart -a -v"
        And gpstart should return a return code of 0
        And all the segments are running
        and the segments are synchronized
        And the backup pid file is deleted on "primary" segment
        And the background pid is killed on "primary" segment

    Scenario: Pid does not correspond to any running process 
        Given the database is running
        And all the segments are running
        and the segments are synchronized
        and the "primary" segment information is saved
        When the postmaster.pid file on "primary" segment is saved
        And the user runs "gpstop -a"
        And gpstop should return a return code of 0
        And we generate the postmaster.pid file with a non running pid on the same "primary" segment
        Then the user runs "gpstart -a"
        And gpstart should return a return code of 0
        And all the segments are running
        and the segments are synchronized
        And the backup pid file is deleted on "primary" segment
 
    Scenario: Starting DB when one mirror segment is already up
        Given the database is running
        And all the segments are running
        and the segments are synchronized
        and the "mirror" segment information is saved
        When the user runs "gpstop -a"
        And gpstop should return a return code of 0
        And the user starts one "mirror" segment
        Then the user runs "gpstart -a"
        And gpstart should return a return code of 0
        And all the segments are running
        and the segments are synchronized

    @wip
    @dca
    Scenario: Standby syncmaster is running with no pgrep command on standby
        Given the database is running
        and all the segments are running
        and the segments are synchronized
        When the user initializes standby master on "smdw"
        Then gpinitstandby should return a return code of 0
        And the user runs "gpstop -am" 
        And gpstop should return a return code of 0
        When the user removes the "pgrep" command on standby
        And the user runs "gpstart -a" 
        Then gpstart should return a return code of 0
        And gpstart should print command "pgrep" is not found to stdout
        And the user restores the "pgrep" command on the standby

