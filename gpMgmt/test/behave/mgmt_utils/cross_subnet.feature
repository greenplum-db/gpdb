@cross_subnet
Feature: Tests for a cross_subnet cluster

    Scenario: gpinitsystem works across subnets
        Given the database is running
         Then the primaries and mirrors including coordinatorStandby are on different subnets
          And all the segments are running
          And the segments are synchronized
          And the mirrors replicate and fail over and back correctly
          And the standby replicates and fails over and back correctly

    Scenario: gpinitstandby works across subnets
        Given the database is not running
          And a working directory of the test as '/tmp/gpinitstandby'
          And a cross-subnet cluster without a standby is created with mirrors on "cdw-1" and "sdw1-1,sdw1-2"
         Then the primaries and mirrors excluding coordinatorStandby are on different subnets

        Given the standby is not initialized
         When running gpinitstandby on host "cdw-1" to create a standby on host "cdw-2"
         Then gpinitstandby should return a return code of 0
          And verify the standby coordinator entries in catalog
          And the primaries and mirrors including coordinatorStandby are on different subnets
          And the standby replicates and fails over and back correctly

    Scenario: gpmovemirrors can move mirrors between subnets
        Given the database is not running
          And a working directory of the test as '/tmp/gpmovemirrors'
          And a cross-subnet cluster without a standby is created with mirrors on "cdw-1" and "sdw1-1,sdw1-2"
         Then the primaries and mirrors excluding coordinatorStandby are on different subnets

        Given a gpmovemirrors cross_subnet input file is created
         When the user runs "gpmovemirrors --input=/tmp/gpmovemirrors_input_cross_subnet"
         Then gpmovemirrors should return a return code of 0
          # Verify that mirrors are functional in the new configuration
          And verify the database has mirrors
          And all the segments are running
          And the segments are synchronized
          And verify that mirror segments are in new cross_subnet configuration
          And the mirrors replicate and fail over and back correctly

     Scenario Outline: gpaddmirrors works cross-subnet with pg_hba.conf using HBA_HOSTNAMES: <hba_hostnames>
        Given the database is not running
          And a working directory of the test as '/tmp/gpaddmirrors'
         When with HBA_HOSTNAMES "<hba_hostnames>" a cross-subnet cluster without a standby is created with no mirrors on "cdw-1" and "sdw1-1, sdw1-2"

         When gpaddmirrors adds mirrors with options "<options>"
         Then verify the database has mirrors
          And the primaries and mirrors excluding coordinatorStandby are on different subnets
          And the mirrors replicate and fail over and back correctly

      Examples:
        | hba_hostnames | options          |
        |  0            |                  |
        |  1            |  --hba-hostnames |

