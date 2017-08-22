@gplog
Feature: Test gplog
    """
    use a behave test for logging verification because unit tests rigs are fussy about mocking stdout
    """
    Scenario: logging to stdout works
        Given gpAdminLogs directory has no "test_logging" files
        Given "bin/gppylib/util/test/test_logging.py" is copied to the install directory
        When the user runs "test_logging.py"
        Then test_logging should return a return code of 0
        And test_logging should print "foobar to stdout and file" to stdout
        And test_logging should print "foobar to stdout and file" to logfile
        And test_logging should not print "blah to file only, not to stdout" to stdout
        And test_logging should print "[WARNING]:-foobar to stdout and file" to logfile
        And test_logging should not print "another to file only, not to stdout" to stdout
        And test_logging should print "[INFO]:-another to file only, not to stdout" to logfile
