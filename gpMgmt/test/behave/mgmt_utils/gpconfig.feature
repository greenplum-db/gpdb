@gpconfig
Feature: gpconfig integration tests

    Scenario: gpconfig -s option with --file option with difference between db and conf file
        # it requires 2 change (-c) operations to prove that we have changed a file,
        # because any existing postgresql.conf file could already have the first value in it a priori
        Given the user runs "gpconfig -c statement_mem -v 130MB"
        Then gpconfig should return a return code of 0
        Given the user runs "gpconfig -c statement_mem -v 129MB"
        Then gpconfig should return a return code of 0
        Then verify that the last line of the master postgres configuration file contains the string "129MB"
        Given the user runs "gpconfig -s statement_mem --file"
        Then gpconfig should return a return code of 0
        Then gpconfig should print "Master[\s]*value: 129MB" to stdout
        # TODO verify all segments also have this string

    Scenario: gpconfig with an empty string passed as a value
        # As above, two calls to gpconfig -c are needed to guarantee a change.
        Given the user runs "gpconfig -c unix_socket_directories -v 'foo'"
        Then gpconfig should return a return code of 0
        Given the user runs "gpconfig -c unix_socket_directories -v ''"
        Then gpconfig should return a return code of 0
        Then verify that the last line of the master postgres configuration file contains the string "unix_socket_directories=''"
        Given the user runs "gpconfig -s unix_socket_directories --file"
        Then gpconfig should return a return code of 0
        Then gpconfig should print "Master[\s]*value: ''" to stdout
        # TODO verify all segments also have this string

