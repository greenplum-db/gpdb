Feature: Replication Slots

  Scenario: A new cluster setup
    Given I have a machine with no cluster
    When I create a cluster
    Then the primaries and mirrors should be replicating using replication slots