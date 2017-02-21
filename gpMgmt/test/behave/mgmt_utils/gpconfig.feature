@gpconfig
Feature: gpconfig integration tests

    Scenario: gpconfig -s option with --file option
        Given the user runs "gpconfig -s shared_buffers --file"
        Then gpconfig should return a return code of 0
        Then gpconfig should print Master[\s]*value: 125MB to stdout

    Scenario: gpconfig -s option with --file option with inconsistency
        Given the user runs "gpconfig -c statement_mem -v 130MB"
        Then gpconfig should return a return code of 0
        Given the user runs "gpconfig -s statement_mem --file"
        Then gpconfig should return a return code of 0
        Then gpconfig should print Master[\s]*value: 130MB to stdout
        #TODO: Remove statement_mem change from postgresql.conf
