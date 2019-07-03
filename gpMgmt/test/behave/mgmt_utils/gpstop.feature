@gpstop
Feature: gpstop behave tests

    @concourse_cluster
    @demo_cluster
    Scenario: gpstop succeeds
        Given the database is running
         When the user runs "gpstop -a"
         Then gpstop should return a return code of 0

    @concourse_cluster
    @demo_cluster
    Scenario: when there are user connections gpstop waits to shutdown until user switches to fast mode
        Given the database is running
          And the user asynchronously runs "psql postgres" and the process is saved
         When the user runs gpstop -a -t 4 --skipvalidation and selects f
          And gpstop should print "'\(s\)mart_mode', '\(f\)ast_mode', '\(i\)mmediate_mode'" to stdout
         Then gpstop should return a return code of 0

    @concourse_cluster
    @demo_cluster
    Scenario: when there are user connections gpstop waits to shutdown until user connections are disconnected
        Given the database is running
          And the user asynchronously runs "psql postgres" and the process is saved
          And the user asynchronously sets up to end that process in 6 seconds
         When the user runs gpstop -a -t 2 --skipvalidation and selects s
          And gpstop should print "There were 1 user connections at the start of the shutdown" to stdout
          And gpstop should print "'\(s\)mart_mode', '\(f\)ast_mode', '\(i\)mmediate_mode'" to stdout
         Then gpstop should return a return code of 0
