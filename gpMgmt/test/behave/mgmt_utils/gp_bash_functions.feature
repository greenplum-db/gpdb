@gp_bash_functions.sh

Feature: gp_bash_funtions.sh unit test cases
    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when no banner is set
        Given a standard local demo cluster is running
        Then source gp_bash_functions and run simple echo

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when single line banner is set
        Given a standard local demo cluster is running
        When the user sets banner on host
        Then source gp_bash_functions and run simple echo

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when multi line banner is set
        Given a standard local demo cluster is running
        When the user sets multi-line banner on host
        Then source gp_bash_functions and run simple echo

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when banner with separator token
        Given a standard local demo cluster is running
        When the user sets banner with separator token on host
        Then source gp_bash_functions and run simple echo

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when single line banner is set for complex command
        Given a standard local demo cluster is running
        When the user sets banner on host
        Then source gp_bash_functions and run complex command

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when multi line banner is set for complex command
        Given a standard local demo cluster is running
        When the user sets multi-line banner on host
        Then source gp_bash_functions and run complex command

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when banner with separator token is set for complex command
        Given a standard local demo cluster is running
        When the user sets banner with separator token on host
        Then source gp_bash_functions and run complex command

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT returns proper output when command output contains separator token
        Given a standard local demo cluster is running
        Then source gp_bash_functions and run echo with separator token

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT called when command output contains separator token and single-line banner is set
        Given a standard local demo cluster is running
        When the user sets banner on host
        Then source gp_bash_functions and run echo with separator token

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT called when command output contains separator token and multi-line banner is set
        Given a standard local demo cluster is running
        When the user sets multi-line banner on host
        Then source gp_bash_functions and run echo with separator token

    @cli_mirrorless
    Scenario: REMOTE_EXECUTE_AND_GET_OUTPUT called when banner with separator token and command output contains separator token
        Given a standard local demo cluster is running
        When the user sets banner with separator token on host
        Then source gp_bash_functions and run echo with separator token
