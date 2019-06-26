@gpstop
Feature: gpstop behave tests

    Scenario: gpstop succeeds
        Given a standard local demo cluster is running
         When the user runs "gpstop -a"
         Then gpstop should return a return code of 0

    Scenario: when there are user connections gpstop waits to shutdown until a user makes a selection
        Given a standard local demo cluster is running
          And the user asynchronously runs "psql postgres" and the process is saved
         When the user runs gpstop -a -t 4 and selects f
          And gpstop should print "'\(s\)mart_mode', '\(f\)ast_mode', '\(i\)mmediate_mode'" to stdout
         Then gpstop should return a return code of 0

    Scenario: when there are user connections gpstop waits to shutdown until a user connections are disconnected
        Given a standard local demo cluster is running
          And the user asynchronously runs "psql postgres" and the process is saved
          And the user asynchronously sets up to end that process in 3 seconds
         When the user runs gpstop -a -t 1 and selects s
          And gpstop should print "'\(s\)mart_mode', '\(f\)ast_mode', '\(i\)mmediate_mode'" to stdout
          Then gpstop should return a return code of 0
