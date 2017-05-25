@gpcheck
Feature: Test gpcheck
    @gpcheck_as_gpadmin
    Scenario: Run gpcheck --local as non-root without ssh permission
        Given user does not have ssh permissions
        When the user runs "gpcheck --local"
        Then gpcheck should return a return code of 0
        And gpcheck should print "gpcheck completing" to stdout
        And user has ssh permissions

    @gpcheck_as_gpadmin
    Scenario: Run gpcheck --host as non root with ssh permissions
        Given we have exchanged keys with the cluster 
        When the user runs "gpcheck --host localhost" 
        Then gpcheck should return a return code of 0
        And gpcheck should print "gpcheck completing" to stdout

    @gpcheck_as_gpadmin
    Scenario: Negative tests cases command line options with --local
        When the user runs "gpcheck --local --host foo"
        Then gpcheck should return a return code of 0
        And gpcheck should print "Only 1 of --file or --host or --local can be specified" to stdout
        When the user runs "gpcheck --local --file foo"
        Then gpcheck should return a return code of 0
        And gpcheck should print "Only 1 of --file or --host or --local can be specified" to stdout
        When the user runs "gpcheck --file bar --host foo"
        Then gpcheck should return a return code of 0
        And gpcheck should print "Only 1 of --file or --host or --local can be specified" to stdout
         
