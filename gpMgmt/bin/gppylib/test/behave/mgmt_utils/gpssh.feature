@gpssh
Feature: gpssh behave tests

    Scenario: gpssh -d and -t options
        When the user runs "gpssh -v -h localhost hostname"
        Then gpssh should return a return code of 0
        And gpssh should print delaybeforesend 0.05 and prompt_validation_timeout 1.0 to stdout
        When the user runs "gpssh -v -h localhost -d 0.051 -t 1.01 hostname"
        Then gpssh should return a return code of 0
        And gpssh should print Skip parsing gpssh.conf to stdout
        When the user runs "gpssh -v -h localhost -d -1 hostname"
        Then gpssh should return a return code of 1
        And gpssh should print delaybeforesend cannot be negative to stdout
        When the user runs "gpssh -v -h localhost -t 0 hostname"
        Then gpssh should return a return code of 1
        And gpssh should print prompt_validation_timeout cannot be negative or zero to stdout

    Scenario: gpssh exceptions
        When the user runs "gpssh -h xfoobarx hostname"
        Then gpssh should return a return code of 0
        And gpssh should print Could not acquire connection to stdout
        When the user runs "gpssh -h localhost -d 0 -t 0.000001 hostname"
        Then gpssh should return a return code of 0
        And gpssh should print unable to login to localhost to stdout
        And gpssh should print could not synchronize with original prompt to stdout
