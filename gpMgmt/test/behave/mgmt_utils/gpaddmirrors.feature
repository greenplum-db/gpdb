@gpaddmirrors
Feature: gpaddmirrors tests

    # Before running any tests in this feature file on your machine, ensure you have done the following:
    # Export $BLDWRAP_TOP=[home directory]
    # Create the directories /sys_mgmt_test/test/general under $BLDWRAP_TOP
    # Create a gpinitsystem config file called "cluster_conf.out" under the new "general" directory
    # Create a hostlist file called "hostlist.out" under the new "general" directory
    # Create a GUC file called "postgresql_param.conf" under the new "general" directory
    # The GUC file must contain a max_resource_queues parameter (default value 100)
    # This procedure is necessary to simulate the file structure of the Pulse machines

    @wip
    Scenario: gpaddmirrors sanity test
        Given the database is running
        And there are no backup files
        When the user runs "gpdeletesystem -d $MASTER_DATA_DIRECTORY < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpdeletesystem should return a return code of 0
        Given the user creates an init config file "/tmp/initsystem_config_primaries.out" without mirrors
        When the user runs "gpinitsystem -c /tmp/initsystem_config_primaries.out -h $BLDWRAP_TOP/sys_mgmt_test/test/general/hostlist.out -p $BLDWRAP_TOP/sys_mgmt_test/test/general/postgresql_param.conf -a"
        Given the user creates mirrors config file "/tmp/config_mirrors"
        When the user runs "gpaddmirrors -m /tmp/config_mirrors -a" 
        Then gpaddmirrors should return a return code of 0
        And the segments are synchronized
        When the user runs "gpdeletesystem -d $MASTER_DATA_DIRECTORY < test/behave/mgmt_utils/steps/data/yes.txt"
        Then gpdeletesystem should return a return code of 0
        When the user runs "gpinitsystem -c $BLDWRAP_TOP/sys_mgmt_test/test/general/cluster_conf.out -h $BLDWRAP_TOP/sys_mgmt_test/test/general/hostlist.out -p $BLDWRAP_TOP/sys_mgmt_test/test/general/postgresql_param.conf -a"
        Then database "testdb" health check should pass on table "t1"
        And the file "/tmp/initsystem_config_primaries.out" is removed from the system
        And the file "/tmp/config_mirrors" is removed from the system
            

