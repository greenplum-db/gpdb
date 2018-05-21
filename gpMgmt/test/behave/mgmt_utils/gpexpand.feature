@gpexpand
Feature: expand the cluster by adding more segments

    @gpexpand_no_mirrors
    @gpexpand_ranks
    @gpexpand_timing
    Scenario: after resuming a duration interrupted redistribution, tables were restored in the user defined order
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1"
        And the user runs gpexpand interview to add 2 new segment and 0 new host "ignored.host"
        And the number of segments have been saved
        And user has created expansionranktest tables
        And 4000000 rows are inserted into table "expansionranktest8" in schema "public" with column type list "int"
        When the user runs gpexpand with the latest gpexpand_inputfile
        And user has fixed the expansion order for tables
        And the user runs gpexpand to redistribute with the --duration flag
        Then gpexpand should return a return code of 0
        And gpexpand should print "End time reached.  Stopping expansion." to stdout
        And verify that the cluster has 2 new segments
        When the user runs gpexpand to redistribute
        Then the tables were expanded in the specified order

    @gpexpand_no_mirrors
    @gpexpand_ranks
    @gpexpand_timing
    Scenario: after resuming an end time interrupted redistribution, tables were restored in the user defined order
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1"
        And the user runs gpexpand interview to add 2 new segment and 0 new host "ignored.host"
        And the number of segments have been saved
        And user has created expansionranktest tables
        And 4000000 rows are inserted into table "expansionranktest8" in schema "public" with column type list "int"
        When the user runs gpexpand with the latest gpexpand_inputfile
        And user has fixed the expansion order for tables
        And the user runs gpexpand to redistribute with the --end flag
        Then gpexpand should return a return code of 0
        And gpexpand should print "End time reached.  Stopping expansion." to stdout
        And verify that the cluster has 2 new segments
        When the user runs gpexpand to redistribute
        Then the tables were expanded in the specified order

    @gpexpand_no_mirrors
    @gpexpand_segment
    Scenario: expand a cluster that has no mirrors
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1"
        And the user runs gpexpand interview to add 2 new segment and 0 new host "ignored.host"
        And the number of segments have been saved
        When the user runs gpexpand with the latest gpexpand_inputfile
        Then gpexpand should return a return code of 0
        And verify that the cluster has 2 new segments

    @gpexpand_no_mirrors
    @gpexpand_host
    Scenario: expand a cluster that has no mirrors with one new hosts
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1,sdw2"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1,sdw2"
        And the new host "sdw2" is ready to go
        And the user runs gpexpand interview to add 0 new segment and 1 new host "sdw2"
        And the number of segments have been saved
        When the user runs gpexpand with the latest gpexpand_inputfile
        Then gpexpand should return a return code of 0
        And verify that the cluster has 2 new segments

    @gpexpand_no_mirrors
    @gpexpand_host_and_segment
    Scenario: expand a cluster that has no mirrors with one new hosts
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1,sdw2"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1,sdw2"
        And the new host "sdw2" is ready to go
        And the user runs gpexpand interview to add 1 new segment and 1 new host "sdw2"
        And the number of segments have been saved
        When the user runs gpexpand with the latest gpexpand_inputfile
        Then gpexpand should return a return code of 0
        And verify that the cluster has 4 new segments

    @gpexpand_mirrors
    @gpexpand_segment
    Scenario: expand a cluster that has mirrors
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1"
        And the number of segments have been saved
        When the user runs gpexpand with a static inputfile for a single-node cluster with mirrors
        Then gpexpand should return a return code of 0
        And verify that the cluster has 4 new segments

    @gpexpand_mirrors
    @gpexpand_host
    Scenario: expand a cluster that has mirrors with one new hosts
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1,sdw2,sdw3"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1,sdw2,sdw3"
        And the new host "sdw2,sdw3" is ready to go
        And the user runs gpexpand interview to add 0 new segment and 2 new host "sdw2,sdw3"
        And the number of segments have been saved
        When the user runs gpexpand with the latest gpexpand_inputfile
        Then gpexpand should return a return code of 0
        And verify that the cluster has 8 new segments

    @gpexpand_mirrors
    @gpexpand_host_and_segment
    Scenario: expand a cluster that has mirrors with one new hosts
        Given a working directory of the test as '/tmp/gpexpand_behave'
        And the database is killed on hosts "mdw,sdw1,sdw2,sdw3"
        And the user runs command "rm -rf /tmp/gpexpand_behave/*"
        And a temporary directory to expand into
        And the database is not running
        And a cluster is created with mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there are no gpexpand_inputfiles
        And the cluster is setup for an expansion on hosts "mdw,sdw1,sdw2,sdw3"
        And the new host "sdw2,sdw3" is ready to go
        And the user runs gpexpand interview to add 1 new segment and 2 new host "sdw2,sdw3"
        And the number of segments have been saved
        When the user runs gpexpand with the latest gpexpand_inputfile
        Then gpexpand should return a return code of 0
        And verify that the cluster has 14 new segments

