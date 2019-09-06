@replication_slots
Feature: Replication Slots

  Scenario: Lifecycle of cluster's replication slots
    Given I have a machine with no cluster
    When I create a cluster
    Then the primaries and mirrors should be replicating using replication slots

    Given a preferred primary has failed
    When the user runs "gprecoverseg -a"
    And gprecoverseg should return a return code of 0
    And the segments are synchronized
    And primary and mirror switch to non-preferred roles
    Then the primaries and mirrors should be replicating using replication slots
    And the mirrors should not have replication slots

    When the user runs "gprecoverseg -ra"
    Then gprecoverseg should return a return code of 0
    And the segments are synchronized
    And the primaries and mirrors should be replicating using replication slots

    When a mirror has crashed
    And the user runs "gprecoverseg -aFv"
    And gprecoverseg should return a return code of 0
    And the segments are synchronized
    Then the primaries and mirrors should be replicating using replication slots

    When I add a segment to the cluster
    And the segments are synchronized
    Then the primaries and mirrors should be replicating using replication slots

  Scenario: A adding mirrors to a cluster after the primaries have been initialized
    Given I cluster with no mirrors
    When I add mirrors to the cluster
    Then the primaries and mirrors should be replicating using replication slots

