-- When I create a replication slot on the master
-- Then I should expect an error warning me that
--   my command is only run on the current segment.
select pg_create_physical_replication_slot('some_replication_slot');

-- And I should see that my replication slot exists
select pg_get_replication_slots();

-- Cleanup:
select pg_drop_replication_slot('some_replication_slot');
