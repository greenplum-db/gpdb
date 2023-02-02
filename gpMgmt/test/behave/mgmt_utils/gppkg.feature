# Note: these tests require the rpm binary to be installed
@gppkg
Feature: gppkg tests

    Scenario: gppkg environment does not have any gppkg
        Given the database is running
        And database "gptest" exists
        Then the user runs "gppkg --remove sample"

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

    Scenario: gppkg --query --all when nothing is installed should report nothing installed
        Given the database is running
        When the user runs "gppkg --query --all"
        Then gppkg should return a return code of 0
        And gppkg should print "Starting gppkg with args: --query --all" to stdout

    Scenario: gppkg --remove should report failure when the package is not installed
        Given the database is running
        When the user runs "gppkg --remove sample"
        Then gppkg should return a return code of 2
        And gppkg should print "Package sample has not been installed" to stdout

########################### @concourse_cluster tests ###########################
# The @concourse_cluster tag denotes the scenario that requires a remote cluster

    @concourse_cluster
    Scenario: gppkg --install should report success because the package is not yet installed
        Given the database is running
        When the user runs "gppkg --install test/behave/mgmt_utils/steps/data/sample.gppkg"
        Then gppkg should return a return code of 0
        And gppkg should print "This is a sample message shown after successful installation" to stdout
        And gppkg should print "Completed local installation of sample" to stdout
        And "sample" gppkg files exist on all hosts

    @concourse_cluster
    Scenario: gppkg --install should report failure because the package is already installed
        Given the database is running
        When the user runs "gppkg --install test/behave/mgmt_utils/steps/data/sample.gppkg"
        Then gppkg should return a return code of 2
        And gppkg should print "sample.gppkg is already installed." to stdout

    @concourse_cluster
    Scenario: gppkg --remove should report success when the package is already installed
        Given the database is running
        When the user runs "gppkg --remove sample"
        Then gppkg should return a return code of 0
        And gppkg should print "Uninstalling package sample.gppkg" to stdout
        And gppkg should print "Completed local uninstallation of sample.gppkg" to stdout
        And gppkg should print "sample.gppkg successfully uninstalled" to stdout
        And "sample" gppkg files do not exist on any hosts

    @concourse_cluster
    Scenario: gppkg --query should report installed packages
        Given the database is running
        # to be idempotent, potentially reinstalling if the above test just ran,
        # which will result in an error result code, so don't check result code of install
        When the user runs "gppkg --install test/behave/mgmt_utils/steps/data/sample.gppkg"
        When the user runs "gppkg --query --all"
        Then gppkg should return a return code of 0
        And gppkg should print "Starting gppkg with args: --query --all" to stdout
        And gppkg should print "sample" to stdout

    @concourse_cluster
    Scenario: gppkg --clean (which should be named "sync") should install to the segment host that lacks a gppkg found elsewhere
        Given the database is running
        When the user runs "gppkg --install test/behave/mgmt_utils/steps/data/sample.gppkg"
        And gppkg "sample" is removed from a segment host
        And the user runs "gppkg --clean"
        Then gppkg should return a return code of 0
        And gppkg should print "The following packages will be installed on .*: sample.gppkg" to stdout
        And "sample" gppkg files exist on all hosts

    @concourse_cluster
    Scenario: gppkg --clean (which should be named "sync") should remove on all segment hosts when gppkg does not exist in coordinator
        Given the database is running
        When the user runs "gppkg --install test/behave/mgmt_utils/steps/data/sample.gppkg"
        And gppkg "sample" is removed from coordinator host
        And the user runs "gppkg --clean"
        Then gppkg should return a return code of 0
        And gppkg should print "The following packages will be uninstalled on .*: sample.gppkg" to stdout
        And "sample" gppkg files do not exist on any hosts

    @concourse_cluster
    Scenario: gppkg --migrate copies all packages from coordinator to all segment hosts
        Given the database is running
        And the user runs "gppkg -r sample"
        And a gphome copy is created at /tmp/gppkg_migrate/ on all hosts
        When a user runs "COORDINATOR_DATA_DIRECTORY=$COORDINATOR_DATA_DIRECTORY gppkg -r sample" with gphome "/tmp/gppkg_migrate"
        And "sample" gppkg files do not exist on any hosts
        When a user runs "COORDINATOR_DATA_DIRECTORY=$COORDINATOR_DATA_DIRECTORY gppkg --install $(pwd)/test/behave/mgmt_utils/steps/data/sample.gppkg" with gphome "/tmp/gppkg_migrate"
        Then gppkg should return a return code of 0
        And "sample" gppkg files do not exist on any hosts
        And the user runs "gpstop -a && gpstart -a -m"
        When the user runs "gppkg --migrate /tmp/gppkg_migrate $GPHOME"
        Then gppkg should return a return code of 0
        And gppkg should print "Installing sample.gppkg locally" to stdout
        And gppkg should print "The following packages will be installed on .*: sample.gppkg" to stdout
        And gppkg should print "Successfully cleaned the cluster" to stdout
        And gppkg should print "The package migration has completed" to stdout
        And the user runs "gpstop -ar"
        And "sample" gppkg files exist on all hosts
