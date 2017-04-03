@gpdeletesystem
Feature: gpdeletesystem tests

    # Before running any tests in this feature file on your machine, ensure you have done the following:
    # Export $BLDWRAP_TOP=[home directory]
    # Create the directories /sys_mgmt_test under $BLDWRAP_TOP
    # Create a gpinitsystem config file called "cluster_conf.out" under the new "sys_mgmt_test" directory
    # Create a hostlist file called "hostlist.out" under the new "sys_mgmt_test" directory
    # Create a GUC file called "postgresql_param.conf" under the new "sys_mgmt_test" directory
    #   The GUC file must contain a max_resource_queues parameter (default value 100)
    # This procedure is necessary to simulate the file structure of the Pulse machines

    Scenario: gpdeletesystem with PGPORT unset
        Given the database is running
        And there are no backup files
        And the environment variable "PGPORT" is not set
        When the user runs "gpdeletesystem -d $MASTER_DATA_DIRECTORY < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpdeletesystem should return a return code of 0
        And the environment variable "PGPORT" is reset
        And the user runs "gpinitsystem -c $BLDWRAP_TOP/sys_mgmt_test/cluster_conf.out -h $BLDWRAP_TOP/sys_mgmt_test/hostlist.out -p $BLDWRAP_TOP/sys_mgmt_test/postgresql_param.conf -a"
        And database "testdb" health check should pass on table "t1"
        
    Scenario: gpdeletesystem with PGPORT set to a different value
        Given the database is running
        And there are no backup files
        And the environment variable "PGPORT" is set to "1234567890"
        When the user runs "gpdeletesystem -d $MASTER_DATA_DIRECTORY < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpdeletesystem should return a return code of 1
        And gpdeletesystem should print PGPORT value in .*/postgresql.conf does not match PGPORT environment variable error message 
        And the environment variable "PGPORT" is reset

    Scenario: gpdeletesystem with PGPORT set
        Given the database is running
        And there are no backup files
        When the user runs "gpdeletesystem -d $MASTER_DATA_DIRECTORY < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpdeletesystem should return a return code of 0
        And the user runs "gpinitsystem -c $BLDWRAP_TOP/sys_mgmt_test/cluster_conf.out -h $BLDWRAP_TOP/sys_mgmt_test/hostlist.out -p $BLDWRAP_TOP/sys_mgmt_test/postgresql_param.conf -a"
        And database "testdb" health check should pass on table "t1"
