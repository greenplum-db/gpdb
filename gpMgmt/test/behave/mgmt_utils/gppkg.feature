@gppkg
Feature: gppkg tests

    Scenario: gppkg -u should prompt user when package is updated with -a option
        Given the database is running
        When the user runs "gppkg -u foo.gppkg -a"
        Then gppkg should return a return code of 2
        And gppkg should print "WARNING: The process of updating a package includes removing all" to stdout
        And gppkg should print "previous versions of the system objects related to the package. For" to stdout
        And gppkg should print "example, previous versions of shared libraries are removed." to stdout
        And gppkg should print "After the update process, a database function will fail when it is" to stdout
        And gppkg should print "called if the function references a package file that has been removed." to stdout
        And gppkg should print "Cannot find package foo.gppkg" to stdout
        And gppkg should not print "Do you still want to continue ?" to stdout

    Scenario: gppkg -u should prompt user when package is updated with yes as input
        Given the database is running
        When the user runs "gppkg -u foo.gppkg < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gppkg should return a return code of 2
        And gppkg should print "WARNING: The process of updating a package includes removing all" to stdout
        And gppkg should print "previous versions of the system objects related to the package. For" to stdout
        And gppkg should print "example, previous versions of shared libraries are removed." to stdout
        And gppkg should print "After the update process, a database function will fail when it is" to stdout
        And gppkg should print "called if the function references a package file that has been removed." to stdout
        And gppkg should print "Cannot find package foo.gppkg" to stdout
        And gppkg should not print "Skipping update of gppkg based on user input" to stdout
        And gppkg should print "Do you still want to continue ?" to stdout

    Scenario: gppkg -u should prompt user when package is updated with no as input
        Given the database is running
        When the user runs "gppkg -u foo.gppkg < test/behave/mgmt_utils/steps/data/no.txt"
        Then gppkg should return a return code of 0
        And gppkg should print "WARNING: The process of updating a package includes removing all" to stdout
        And gppkg should print "previous versions of the system objects related to the package. For" to stdout
        And gppkg should print "example, previous versions of shared libraries are removed." to stdout
        And gppkg should print "After the update process, a database function will fail when it is" to stdout
        And gppkg should print "called if the function references a package file that has been removed." to stdout
        And gppkg should print "Skipping update of gppkg based on user input" to stdout
        And gppkg should print "Do you still want to continue ?" to stdout

    @gppkg_query
    Scenario: gppkg --query --all when nothing is installed should report nothing installed
        Given the database is running
        When the user runs "gppkg --query --all"
        Then gppkg should return a return code of 0
        And gppkg should print "Starting gppkg with args: --query --all" to stdout
