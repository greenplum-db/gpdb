@gpactivatestandby
Feature: gpactivatestandby stop segments
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
        And gpstart should print "Standby Master could not be started" to stdout
        And the user stops the syncmaster
