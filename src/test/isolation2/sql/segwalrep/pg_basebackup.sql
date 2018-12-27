include: helpers/gp_management_utils_helpers.sql;

-- Given a segment running without a replication slot
0U: select * from pg_drop_replication_slot('some_replication_slot');
!\retcode rm -rf /tmp/some_isolation2_pg_basebackup;

-- When pg_basebackup runs with --slot and stream as xlog-method
select pg_basebackup(address, port, 'some_replication_slot', '/tmp/some_isolation2_pg_basebackup') from gp_segment_configuration where content = 0 and role = 'p';

-- Then a replication slot gets created on that segment with the slot
-- name and the slot's restart_lsn is not NULL, indicating that the
-- WAL streamer process utilized this slot for streaming WAL during
-- base backup
0U: select slot_name, slot_type, active, restart_lsn is not NULL as slot_was_used from pg_get_replication_slots() where slot_name = 'some_replication_slot';

-- Given we remove the replication slot
0U: select * from pg_drop_replication_slot('some_replication_slot');
!\retcode rm -rf /tmp/some_isolation2_pg_basebackup;

-- When pg_basebackup runs without --slot
select pg_basebackup(address, port, null, '/tmp/some_isolation2_pg_basebackup') from gp_segment_configuration where content = 0 and role = 'p';

-- Then there should NOT be a replication slot
0U: select count(1) from pg_get_replication_slots() where slot_name = 'some_replication_slot';

-- Given that we suspend a WAL sender
create extension if not exists gp_inject_fault;
select gp_inject_fault_infinite('wal_sender_loop', 'suspend', 2);
!\retcode rm -rf /tmp/some_isolation2_pg_basebackup;

-- And we make a call pg_basebackup that will lead to WAL sender fork
1&:select pg_basebackup(address, port, null, '/tmp/some_isolation2_pg_basebackup') from gp_segment_configuration where content = 0 and role = 'p';
SELECT gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid) FROM gp_segment_configuration WHERE content = 0 AND role = 'p';

-- Then we expect that the application_name of the  WAL sender is not the same
-- 'gp_walreceiver' application name used by primary/mirror WAL sender
0U: select count(application_name) > 0 as has_replication_app_names from pg_stat_replication where application_name <> 'gp_walreceiver';

select gp_inject_fault('wal_sender_loop', 'resume', 2);
select gp_inject_fault('wal_sender_loop', 'reset', 2);

1<:

