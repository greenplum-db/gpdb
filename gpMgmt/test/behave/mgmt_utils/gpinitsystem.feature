@gpinitsystem
Feature: gpinitsystem tests

    Scenario: gpinitsystem creates a cluster with data_checksums on
        Given the database is initialized with checksum "on"
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        And gpconfig should print "Values on all segments are consistent" to stdout
        And gpconfig should print "Master  value: on" to stdout
        And gpconfig should print "Segment value: on" to stdout

    Scenario: gpinitsystem creates a cluster with data_checksums off
        Given the database is initialized with checksum "off"
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        And gpconfig should print "Values on all segments are consistent" to stdout
        And gpconfig should print "Master  value: off" to stdout
        And gpconfig should print "Segment value: off" to stdout

    Scenario: gpinitsystem exits with status 1 when the user enters nothing for the confirmation
        Given create demo cluster config
         When the user runs command "echo '' | gpinitsystem -c ../gpAux/gpdemo/clusterConfigFile" eok
         Then gpinitsystem should return a return code of 1
        Given the user runs "gpstate"
         Then gpstate should return a return code of 2

    Scenario: gpinitsystem exits with status 1 when the user enters no for the confirmation
        Given create demo cluster config
         When the user runs command "echo no | gpinitsystem -c ../gpAux/gpdemo/clusterConfigFile" eok
         Then gpinitsystem should return a return code of 1
        Given the user runs "gpstate"
         Then gpstate should return a return code of 2

    Scenario: gpinitsystem creates a cluster when the user confirms the dialog
        Given create demo cluster config
         When the user runs command "echo y | gpinitsystem -c ../gpAux/gpdemo/clusterConfigFile"
         Then gpinitsystem should return a return code of 0
        Given the user runs "gpstate"
         Then gpstate should return a return code of 0

    Scenario: gpinitsystem fails with exit code 1 when the functions file is not found
       Given create demo cluster config
           # force a load error when trying to source gp_bash_functions.sh
        When the user runs command "ln -s -f `which gpinitsystem` /tmp/gpinitsystem-link; . /tmp/gpinitsystem-link" eok
        Then gpinitsystem should return a return code of 1
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile"
        Then gpinitsystem should return a return code of 0

    Scenario: gpinitsystem continues running when gpinitstandby fails
       Given create demo cluster config
           # force gpinitstandby to fail by specifying a directory that does not exist (gpinitsystem continues successfully)
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -s localhost -S not-a-real-directory -P 21100 -h ../gpAux/gpdemo/hostfile"
        Then gpinitsystem should return a return code of 0

    Scenario: after a failed run of gpinitsystem, a re-run should return exit status 0
       Given create demo cluster config
           # force a failure by passing no args
        When the user runs "gpinitsystem"
        Then gpinitsystem should return a return code of 1
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile"
        Then gpinitsystem should return a return code of 0

    Scenario: after gpinitsystem logs a warning, a re-run should return exit status 0
      Given create demo cluster config
          # log a warning
        And the user runs command "echo 'ARRAY_NAME=' >> ../gpAux/gpdemo/clusterConfigFile"
       When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile"
       Then gpinitsystem should return a return code of 0
      Given create demo cluster config
       When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile"
       Then gpinitsystem should return a return code of 0

    Scenario: gpinitsystem should warn but not fail when standby cannot be instantiated
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the standby is not initialized
        And the user runs command "rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest"
        # stop db and make sure cluster config exists so that we can manually initialize standby
        And the cluster config is generated with data_checksums "1"
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -s localhost -P 21100 -S /wrong/path -h ../gpAux/gpdemo/hostfile"
        Then gpinitsystem should return a return code of 0
        And gpinitsystem should not print "To activate the Standby Master Segment in the event of Master" to stdout
        And gpinitsystem should print "Cluster setup finished, but Standby Master failed to initialize. Review contents of log files for errors." to stdout
        And sql "select * from gp_toolkit.__gp_user_namespaces" is executed in "postgres" db

    Scenario: gpinitsystem generates an output configuration file and then starts cluster with data_checksums on
        Given the cluster config is generated with data_checksums "on"
        When the user runs command "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -O /tmp/output_config_file"
        And gpinitsystem should return a return code of 0
        Then verify that file "output_config_file" exists under "/tmp"
        And verify that the file "/tmp/output_config_file" contains "HEAP_CHECKSUM=on"
        And the user runs "gpinitsystem -a -I /tmp/output_config_file -l /tmp/"
        Then gpinitsystem should return a return code of 0
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        And gpconfig should print "Values on all segments are consistent" to stdout
        And gpconfig should print "Master  value: on" to stdout
        And gpconfig should print "Segment value: on" to stdout

    Scenario: gpinitsystem generates an output configuration file and then starts cluster with data_checksums off
        Given the cluster config is generated with data_checksums "off"
        When the user runs command "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -O /tmp/output_config_file"
        And gpinitsystem should return a return code of 0
        Then verify that file "output_config_file" exists under "/tmp"
        And verify that the file "/tmp/output_config_file" contains "HEAP_CHECKSUM=off"
        And the user runs "gpinitsystem -a -I /tmp/output_config_file -l /tmp/"
        Then gpinitsystem should return a return code of 0
        When the user runs "gpconfig -s data_checksums"
        Then gpconfig should return a return code of 0
        And gpconfig should print "Values on all segments are consistent" to stdout
        And gpconfig should print "Master  value: off" to stdout
        And gpconfig should print "Segment value: off" to stdout

    Scenario: gpinitsystem should warn but not fail when standby cannot be instantiated
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the standby is not initialized
        And the user runs command "rm -rf $MASTER_DATA_DIRECTORY/newstandby"
        And the user runs command "rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest"
        And the cluster config is generated with data_checksums "1"
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -l /tmp/gpinitsystemtest -s localhost -P 21100 -S $MASTER_DATA_DIRECTORY/newstandby -h ../gpAux/gpdemo/hostfile"
        Then gpinitsystem should return a return code of 0
        And gpinitsystem should print "Log file scan check passed" to stdout
        And sql "select * from gp_toolkit.__gp_user_namespaces" is executed in "postgres" db

    @skip_fixme_ubuntu18.04
    Scenario: gpinitsystem creates a cluster in default timezone
        Given the database is not running
        And "TZ" environment variable is not set
        And the system timezone is saved
        And the user runs command "rm -rf ../gpAux/gpdemo/datadirs/*"
        And the user runs command "mkdir ../gpAux/gpdemo/datadirs/qddir; mkdir ../gpAux/gpdemo/datadirs/dbfast1; mkdir ../gpAux/gpdemo/datadirs/dbfast2; mkdir ../gpAux/gpdemo/datadirs/dbfast3"
        And the user runs command "mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror1; mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror2; mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror3"
        And the user runs command "rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest"
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -l /tmp/gpinitsystemtest -P 21100 -h ../gpAux/gpdemo/hostfile"
        And gpinitsystem should return a return code of 0
        Then the database timezone is saved
        And the database timezone matches the system timezone
        And the startup timezone is saved
        And the startup timezone matches the system timezone

    Scenario: gpinitsystem creates a cluster using TZ
        Given the database is not running
        And the environment variable "TZ" is set to "US/Hawaii"
        And the user runs command "rm -rf ../gpAux/gpdemo/datadirs/*"
        And the user runs command "mkdir ../gpAux/gpdemo/datadirs/qddir; mkdir ../gpAux/gpdemo/datadirs/dbfast1; mkdir ../gpAux/gpdemo/datadirs/dbfast2; mkdir ../gpAux/gpdemo/datadirs/dbfast3"
        And the user runs command "mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror1; mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror2; mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror3"
        And the user runs command "rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest"
        When the user runs "gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -l /tmp/gpinitsystemtest -P 21100 -h ../gpAux/gpdemo/hostfile"
        And gpinitsystem should return a return code of 0
        Then the database timezone is saved
        And the database timezone matches "HST"
        And the startup timezone is saved
        And the startup timezone matches "HST"

    Scenario: gpinitsystem should print FQDN in pg_hba.conf when HBA_HOSTNAMES=1
        Given the cluster config is generated with HBA_HOSTNAMES "1"
        When generate cluster config file "/tmp/output_config_file"
        Then verify that the file "/tmp/output_config_file" contains "HBA_HOSTNAMES=1"
        When initialize a cluster using "/tmp/output_config_file"
        Then verify that the file "../gpAux/gpdemo/datadirs/qddir/demoDataDir-1/pg_hba.conf" contains FQDN only for trusted host
        And verify that the file "../gpAux/gpdemo/datadirs/dbfast1/demoDataDir0/pg_hba.conf" contains FQDN only for trusted host

    Scenario: gpinitsystem should print CIDR in pg_hba.conf when HBA_HOSTNAMES=0
        Given the cluster config is generated with HBA_HOSTNAMES "0"
        When generate cluster config file "/tmp/output_config_file"
        Then verify that the file "/tmp/output_config_file" contains "HBA_HOSTNAMES=0"
        When initialize a cluster using "/tmp/output_config_file"
        Then verify that the file "../gpAux/gpdemo/datadirs/qddir/demoDataDir-1/pg_hba.conf" contains CIDR only for trusted host
        And verify that the file "../gpAux/gpdemo/datadirs/dbfast1/demoDataDir0/pg_hba.conf" contains CIDR only for trusted host

    Scenario: gpinitsystem should print FQDN in pg_hba.conf for standby when HBA_HOSTNAMES=1
        Given the database is running
        And all the segments are running
        And the segments are synchronized
        And the standby is not initialized
        And ensure the standby directory does not exist
        And the cluster config is generated with HBA_HOSTNAMES "1"
        When generate cluster config file "/tmp/output_config_file"
        Then verify that the file "/tmp/output_config_file" contains "HBA_HOSTNAMES=1"
        When initialize a cluster with standby using "/tmp/output_config_file"
        Then verify that the file "../gpAux/gpdemo/datadirs/qddir/demoDataDir-1/pg_hba.conf" contains FQDN only for trusted host
        And verify that the file "../gpAux/gpdemo/datadirs/dbfast1/demoDataDir0/pg_hba.conf" contains FQDN only for trusted host
        And verify that the file "../gpAux/gpdemo/datadirs/qddir/demoDataDir-1/newstandby/pg_hba.conf" contains FQDN only for trusted host

    Scenario: gpinitsystem on a DCA system is able to set the DCA specific GUCs
	Given create demo cluster config
        And the user runs command "rm -r ~/gpAdminLogs/gpinitsystem*"
        And a working directory of the test as '/tmp/gpinitsystem'
        # create a dummy dca version file so that DCA specific parameters are set
        And the user runs command "touch /tmp/gpinitsystem/gpdb-appliance-version"
        When the user runs command "source $GPHOME/greenplum_path.sh; __DCA_VERSION_FILE__=/tmp/gpinitsystem/gpdb-appliance-version $GPHOME/bin/gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile"
        Then gpinitsystem should return a return code of 0
        # the log file must have the entry indicating that DCA specific configuration has been set
        And the user runs command "egrep 'Setting DCA specific configuration values' ~/gpAdminLogs/gpinitsystem*log"
