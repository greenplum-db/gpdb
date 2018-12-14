-- Given a segment running without a replication slot
select * from pg_drop_replication_slot('some_replication_slot');

\! rm -rf /tmp/some_pg_basebackup

-- When pg_basebackup runs with --slot
\! pg_basebackup --slot='some_replication_slot' --xlog-method=stream -R -D /tmp/some_pg_basebackup

-- Then a replication slot gets created on that segment with the slot name
select slot_name, slot_type, active from pg_get_replication_slots();

-- Given we remove the replication slot

select * from pg_drop_replication_slot('some_replication_slot');
\! rm -rf /tmp/some_pg_basebackup

-- When pg_basebackup runs without --slot

\! pg_basebackup --xlog-method=stream -R -D /tmp/some_pg_basebackup

-- Then there should NOT be a replication slot

select count(1) from pg_get_replication_slots();
