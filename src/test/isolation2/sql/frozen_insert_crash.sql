-- Test server crash in case of frozen insert. Make sure that after crash
-- recovery, the frozen insert and index are consistent:
-- 
-- 1. If crash happened before the row is frozen, the row will be invisible;
-- 2. If crash happened after the row is frozen, the row will be visible.
-- 
-- And the above behavior should remain consistent using seqscan or indexscan.
--
-- We test gp_fastsequence here since it does frozen insert and has an index.

-- Case 1. crash after the regular MVCC insert has made to disk, but not
-- the WAL record responsible for updating it to frozen.
-- After crash recovery, the insert will follow regular MVCC and not be seen.
1: create table tab_fi(a int) with (appendoptimized=true) distributed replicated;

-- switch WAL on seg0 to reduce flakiness
1: select gp_segment_id, pg_switch_wal() is not null from gp_dist_random('gp_id') where gp_segment_id = 0;

-- suspend right after the insert into gp_fastsequence during an AO table insert,
-- but before freezing the tuple
1: select gp_inject_fault('insert_fastsequence_before_freeze', 'suspend', ''/*DDL*/, ''/*DB*/, 'gp_fastsequence'/*table*/, 1/*start occur*/, 1/*end occur*/, 0/*extra_arg*/, dbid) from gp_segment_configuration where role = 'p' and content = 0;

2>: insert into tab_fi values(1);

1: select gp_wait_until_triggered_fault('insert_fastsequence_before_freeze', 1, dbid) from gp_segment_configuration where role = 'p' and content = 0;

-- switch WAL on seg0, so the new row gets flushed (including its index)
1: select gp_segment_id, pg_switch_wal() is not null from gp_dist_random('gp_id') where gp_segment_id = 0;

-- inject a panic, and resume the insert. The WAL for the freeze operation is not
-- going to be made to disk (we just flushed WALs), so we won't replay it during restart later.
-- skip FTS probe to prevent unexpected mirror promotion
1: select gp_inject_fault_infinite('fts_probe', 'skip', dbid) from gp_segment_configuration where role='p' and content=-1;
1: select gp_inject_fault('appendonly_insert', 'panic', ''/*DDL*/, ''/*DB*/, 'tab_fi'/*table*/, 1/*start occur*/, -1/*end occur*/, 0/*extra_arg*/, 2/*db_id*/);
1: select gp_inject_fault('insert_fastsequence_before_freeze', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;
1: select gp_inject_fault('fts_probe', 'reset', dbid) from gp_segment_configuration where role='p' and content=-1;

2<:

1q:

-- check the gp_fastsequence content w/ table vs index scan, neither should see the 
-- new inserted row (objmod=1) following MVCC
1: set enable_indexscan = off;
1: set enable_seqscan = on;
1: select distinct f.gp_segment_id, f.objmod, f.last_sequence from gp_dist_random('gp_fastsequence') f left join gp_dist_random('pg_appendonly') a on segrelid = objid and a.gp_segment_id = f.gp_segment_id where a.gp_segment_id = 0 and relid = (select oid from pg_class where relname = 'tab_fi');
1: set enable_indexscan = on;
1: set enable_seqscan = off;
1: select distinct f.gp_segment_id, f.objmod, f.last_sequence from gp_dist_random('gp_fastsequence') f left join gp_dist_random('pg_appendonly') a on segrelid = objid and a.gp_segment_id = f.gp_segment_id where a.gp_segment_id = 0 and relid = (select oid from pg_class where relname = 'tab_fi');
1: reset enable_indexscan;
1: reset enable_seqscan;

1: drop table tab_fi;

-- Case 2. crash after we have flushed the WAL that updates the row to be frozen.
-- After crash recovery, the insert should be seen.
1: create table tab_fi(a int) with (appendoptimized=true) distributed replicated;

-- switch WAL on seg0 to reduce flakiness
1: select gp_segment_id, pg_switch_wal() is not null from gp_dist_random('gp_id') where gp_segment_id = 0;

-- suspend right after freezing the tuple
1: select gp_inject_fault('insert_fastsequence_after_freeze', 'suspend', ''/*DDL*/, ''/*DB*/, 'gp_fastsequence'/*table*/, 1/*start occur*/, 1/*end occur*/, 0/*extra_arg*/, dbid) from gp_segment_configuration where role = 'p' and content = 0;

2>: insert into tab_fi values(1);

1: select gp_wait_until_triggered_fault('insert_fastsequence_after_freeze', 1, dbid) from gp_segment_configuration where role = 'p' and content = 0;

-- switch WAL on seg0, so the freeze record gets flushed
1: select gp_segment_id, pg_switch_wal() is not null from gp_dist_random('gp_id') where gp_segment_id = 0;

-- While we are on it, check the wal record for the freeze operation.
-- One of the purposes is to guard against unexpected addition to the xl_heap_freeze_tuple struct in future.
-- If this test failed due to WAL size, please check to see if the xl_heap_freeze_tuple struct
-- has changed, and if we should initialize any new field in heap_freeze_tuple_no_cutoff().
! seg0_datadir=$(psql -At -c "select datadir from gp_segment_configuration where content = 0 and role = 'p'" postgres) && seg0_last_wal_file=$(psql -At -c "SELECT pg_walfile_name(pg_current_wal_lsn()) from gp_dist_random('gp_id') where gp_segment_id = 0" postgres) && pg_waldump ${seg0_last_wal_file} -p ${seg0_datadir}/pg_wal | grep FREEZE_PAGE;

-- inject a panic and resume in same way as Case 1. But this time we will be able to replay the frozen insert.
-- skip FTS probe to prevent unexpected mirror promotion
1: select gp_inject_fault_infinite('fts_probe', 'skip', dbid) from gp_segment_configuration where role='p' and content=-1;
1: select gp_inject_fault('appendonly_insert', 'panic', ''/*DDL*/, ''/*DB*/, 'tab_fi'/*table*/, 1/*start occur*/, -1/*end occur*/, 0/*extra_arg*/, 2/*db_id*/);
1: select gp_inject_fault('insert_fastsequence_after_freeze', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;
1: select gp_inject_fault('fts_probe', 'reset', dbid) from gp_segment_configuration where role='p' and content=-1;

2<:

1q:

-- check the gp_fastsequence content w/ table vs index scan, both should see the new inserted row (objmod=1)
1: set enable_indexscan = off;
1: set enable_seqscan = on;
1: select distinct f.gp_segment_id, f.objmod, f.last_sequence from gp_dist_random('gp_fastsequence') f left join gp_dist_random('pg_appendonly') a on segrelid = objid and a.gp_segment_id = f.gp_segment_id where a.gp_segment_id = 0 and relid = (select oid from pg_class where relname = 'tab_fi');
1: set enable_indexscan = on;
1: set enable_seqscan = off;
1: select distinct f.gp_segment_id, f.objmod, f.last_sequence from gp_dist_random('gp_fastsequence') f left join gp_dist_random('pg_appendonly') a on segrelid = objid and a.gp_segment_id = f.gp_segment_id where a.gp_segment_id = 0 and relid = (select oid from pg_class where relname = 'tab_fi');
1: reset enable_indexscan;
1: reset enable_seqscan;

1: drop table tab_fi;

-- validate that we've actually tested desired scan method
-- for some reason this disrupts the output of subsequent queries so
-- validating at the end here
! psql postgres -At -c "set enable_indexscan = off; set enable_seqscan = on; explain (costs off) select distinct f.gp_segment_id, f.objmod, f.last_sequence from gp_dist_random('gp_fastsequence') f left join gp_dist_random('pg_appendonly') a on segrelid = objid and a.gp_segment_id = f.gp_segment_id where a.gp_segment_id = 0 and relid = (select oid from pg_class where relname = 'tab_fi');" | grep "Seq Scan on gp_fastsequence";
! psql postgres -At -c "set enable_indexscan = on; set enable_seqscan = off; explain (costs off) select distinct f.gp_segment_id, f.objmod, f.last_sequence from gp_dist_random('gp_fastsequence') f left join gp_dist_random('pg_appendonly') a on segrelid = objid and a.gp_segment_id = f.gp_segment_id where a.gp_segment_id = 0 and relid = (select oid from pg_class where relname = 'tab_fi');" | grep "Index Scan using gp_fastsequence";

