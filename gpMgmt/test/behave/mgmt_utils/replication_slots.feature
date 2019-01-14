@replication_slots
Feature: Replication Slots

  Scenario: Lifecycle of cluster's replication slots
    Given I have a machine with no cluster
    When I create a cluster
    Then the primaries and mirrors should be replicating using replication slots

    Given a preferred primary has failed
    When primary and mirror switch to non-preferred roles
    Then the primaries and mirrors should be replicating using replication slots

    When the user runs "gprecoverseg -ra"
    Then gprecoverseg should return a return code of 0


  Scenario: A adding mirrors to a cluster after the primaries have been initialized
    Given I cluster with no mirrors
    When I add mirrors to the cluster
    Then the primaries and mirrors should be replicating using replication slots

  Scenario: A full recovery of a mirror
    Given I create a cluster
    And a mirror has crashed
    When I fully recover a mirror
    Then the primaries and mirrors should be replicating using replication slots
