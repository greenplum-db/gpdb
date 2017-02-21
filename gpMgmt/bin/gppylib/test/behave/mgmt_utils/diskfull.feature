#
# Copyright (c) Greenplum Inc 2016. All Rights Reserved.
#
# This is a test for the gp_diskusage_soft_limit feature
#
# This test is meant to be run on a single-node systems.


@diskfull
Feature: gp_diskusage_soft_limit testing

  Scenario: Set gp_diskusage_soft_limit to a very low value
    Given the database is running
    And the user runs "gpconfig -s gp_diskusage_soft_limit"
    And gp_diskusage_soft_limit is stored
    And the user sets gp_diskusage_soft_limit to a very low value
    Then the log file has the soft limit exceeded warning
    And the gp_diskusage_soft_limit is reset to the original value

