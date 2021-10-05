@gp_bash_functions.sh
Feature: gp_bash_funtions.sh unit test cases
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when no banner is set
        Given a standard local demo cluster is running
        Then source gp_bash_functions and run simple echo

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when single line banner is set
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets banner on host
        Then source gp_bash_functions and run simple echo
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when multi line banner is set
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets multi-line banner on host
        Then source gp_bash_functions and run simple echo
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when banner with separator token
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets banner with separator token on host
        Then source gp_bash_functions and run simple echo
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when single line banner is set for complex command
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets banner on host
        Then source gp_bash_functions and run complex command
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when multi line banner is set for complex command
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets multi-line banner on host
        Then source gp_bash_functions and run complex command
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when banner with separator token is set for complex command
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets banner with separator token on host
        Then source gp_bash_functions and run complex command
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when command output contains separator token
        Given a standard local demo cluster is running
        Then source gp_bash_functions and run echo with separator token

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT called when command output contains separator token and single-line banner is set
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets banner on host
        Then source gp_bash_functions and run echo with separator token
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT called when command output contains separator token and multi-line banner is set
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets multi-line banner on host
        Then source gp_bash_functions and run echo with separator token
        And restore back bashrc file

    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT called when banner with tolek set and command output contains separator token
        Given a standard local demo cluster is running
        And backup the current bashrc file
        When the user sets banner with separator token on host
        Then source gp_bash_functions and run echo with separator token
        And restore back bashrc file
