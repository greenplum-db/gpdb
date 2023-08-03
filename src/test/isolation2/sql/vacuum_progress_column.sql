-- @Description Test to ensure we correctly report progress in
-- pg_stat_progress_create_vacuum for append-optimized tables.

set default_table_access_method=ao_column;

-- Setup the append-optimized table to be vacuumed
DROP TABLE IF EXISTS vacuum_progress_ao_column;
CREATE TABLE vacuum_progress_ao_column(i int, j int);

-- Add two indexes to be vacuumed as well
CREATE INDEX on vacuum_progress_ao_column(i);
CREATE INDEX on vacuum_progress_ao_column(j);

-- Insert all tuples to seg1 from two current sessions so that data are stored
-- in two segment files.
1: BEGIN;
2: BEGIN;
1: INSERT INTO vacuum_progress_ao_column SELECT 0, i FROM generate_series(1, 100000) i;
2: INSERT INTO vacuum_progress_ao_column SELECT 0, i FROM generate_series(1, 100000) i;
-- Commit so that the logical EOF of segno 2 is non-zero.
2: COMMIT;
2: BEGIN;
2: INSERT INTO vacuum_progress_ao_column SELECT 0, i FROM generate_series(1, 100000) i;
-- Abort so that segno 2 has dead tuples after its logical EOF
2: ABORT;
2q:
-- Abort so that segno 1 has logical EOF = 0.
1: ABORT;

-- Also delete half of the tuples evenly before the EOF of segno 2.
DELETE FROM vacuum_progress_ao_column where j % 2 = 0;

-- Perform VACUUM and observe the progress

-- Suspend execution at pre-cleanup phase after truncating both segfiles to their logical EOF.
SELECT gp_inject_fault('appendonly_after_truncate_segment_file', 'suspend', '', '', '', 2, 2, 0, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1: set Debug_appendonly_print_compaction to on;
1&: VACUUM vacuum_progress_ao_column;
SELECT gp_wait_until_triggered_fault('appendonly_after_truncate_segment_file', 2, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- We are in pre_cleanup phase and some blocks should've been vacuumed by now
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;

-- Resume execution and suspend again in the middle of compact phase
SELECT gp_inject_fault('appendonly_insert', 'suspend', '', '', '', 200, 200, 0, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('appendonly_after_truncate_segment_file', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_wait_until_triggered_fault('appendonly_insert', 200, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- We are in compact phase. num_dead_tuples should increase as we move and count tuples, one by one.
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;

-- Resume execution and suspend again after compacting all segfiles
SELECT gp_inject_fault('vacuum_ao_after_compact', 'suspend', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('appendonly_insert', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_wait_until_triggered_fault('vacuum_ao_after_compact', 1, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- After compacting all segfiles we expect 50000 dead tuples
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;

-- Resume execution and entering post_cleaup phase, suspend at the end of it.
SELECT gp_inject_fault('vacuum_ao_post_cleanup_end', 'suspend', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('vacuum_ao_after_compact', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_wait_until_triggered_fault('vacuum_ao_post_cleanup_end', 1, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- We should have skipped recycling the awaiting drop segment because the segment was still visible to the SELECT gp_wait_until_triggered_fault query.
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;
SELECT gp_inject_fault('vacuum_ao_post_cleanup_end', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
1<:

-- pg_class and collected stats view should be updated after the 1st VACUUM
1U: SELECT wait_until_dead_tup_change_to('vacuum_progress_ao_column'::regclass::oid, 0);
SELECT relpages, reltuples, relallvisible FROM pg_class where relname = 'vacuum_progress_ao_column';
SELECT n_live_tup, n_dead_tup, last_vacuum is not null as has_last_vacuum, vacuum_count FROM gp_stat_all_tables WHERE relname = 'vacuum_progress_ao_column' and gp_segment_id = 1;


-- Perform VACUUM again to recycle the remaining awaiting drop segment marked by the previous run.
SELECT gp_inject_fault('vacuum_ao_after_index_delete', 'suspend', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
1&: VACUUM vacuum_progress_ao_column;
-- Resume execution and entering pre_cleanup phase, suspend at vacuuming indexes.
SELECT gp_inject_fault('vacuum_ao_after_compact', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_wait_until_triggered_fault('vacuum_ao_after_index_delete', 1, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- We are in vacuuming indexes phase (part of ao pre_cleanup phase), index_vacuum_count should increase to 1.
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;

-- Resume execution and moving on to truncate segments that were marked as AWAITING_DROP, there should be only 1.
SELECT gp_inject_fault('appendonly_after_truncate_segment_file', 'suspend', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('vacuum_ao_after_index_delete', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_wait_until_triggered_fault('appendonly_after_truncate_segment_file', 1, dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- We are in post_cleanup phase and should have truncated the old segfile. Both indexes should be vacuumed by now, and heap_blks_vacuumed should also increased
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;

SELECT gp_inject_fault('appendonly_after_truncate_segment_file', 'reset', dbid) FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
1<:

-- Vacuum has finished, nothing should show up in the view.
1U: select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from pg_stat_progress_vacuum;

-- pg_class and collected stats view should be updated after the 2nd VACUUM
1U: SELECT wait_until_dead_tup_change_to('vacuum_progress_ao_column'::regclass::oid, 0);
SELECT relpages, reltuples, relallvisible FROM pg_class where relname = 'vacuum_progress_ao_column';
SELECT n_live_tup, n_dead_tup, last_vacuum is not null as has_last_vacuum, vacuum_count FROM gp_stat_all_tables WHERE relname = 'vacuum_progress_ao_column' and gp_segment_id = 1;

1q:
-- Test vacuum worker process is changed at post-cleanup phase due to mirror down.
-- Current behavior is it will clear previous compact phase num_dead_tuples in post-cleanup
-- phase (at injecting point vacuum_ao_post_cleanup_end), which is different from above case
-- in which vacuum worker isn't changed.
DROP TABLE IF EXISTS vacuum_progress_ao_column;
CREATE TABLE vacuum_progress_ao_column(i int, j int);
CREATE INDEX on vacuum_progress_ao_column(i);
CREATE INDEX on vacuum_progress_ao_column(j);
1: BEGIN;
2: BEGIN;
1: INSERT INTO vacuum_progress_ao_column SELECT i, i FROM generate_series(1, 100000) i;
2: INSERT INTO vacuum_progress_ao_column SELECT i, i FROM generate_series(1, 100000) i;
2: COMMIT;
2: BEGIN;
2: INSERT INTO vacuum_progress_ao_column SELECT i, i FROM generate_series(1, 100000) i;
2: ABORT;
2q:
1: ABORT;
DELETE FROM vacuum_progress_ao_column where j % 2 = 0;

-- Suspend execution at the end of compact phase.
2: SELECT gp_inject_fault('vacuum_ao_after_compact', 'suspend', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

1: set debug_appendonly_print_compaction to on;
1&: vacuum vacuum_progress_ao_column;

2: SELECT gp_wait_until_triggered_fault('vacuum_ao_after_compact', 1, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- Non-zero progressing data num_dead_tuples is showed up.
select gp_segment_id, relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from gp_stat_progress_vacuum where gp_segment_id > -1;
select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from gp_stat_progress_vacuum_summary;

-- Resume execution of compact phase and block at syncrep on one segment.
2: SELECT gp_inject_fault_infinite('wal_sender_loop', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;
2: SELECT gp_inject_fault('vacuum_ao_after_compact', 'reset', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
-- stop the mirror should turn off syncrep
2: SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE content = 1 AND role = 'm';

-- Resume walsender to detect mirror down and suspend at the beginning
-- of post-cleanup taken over by a new vacuum worker.
2: SELECT gp_inject_fault('vacuum_worker_changed', 'suspend', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
-- resume walsender and let it exit so that mirror stop can be detected
2: SELECT gp_inject_fault_infinite('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;
-- Ensure we enter into the target logic which stops cumulative data but
-- initializes a new vacrelstats at the beginning of post-cleanup phase.
-- Also all segments should reach to the same "vacuum_worker_changed" point
-- due to FTS version being changed.
2: SELECT gp_wait_until_triggered_fault('vacuum_worker_changed', 1, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
-- now seg1's mirror is marked as down
2: SELECT content, role, preferred_role, mode, status FROM gp_segment_configuration WHERE content > -1;

-- Resume execution and entering post_cleaup phase, suspend at the end of it.
2: SELECT gp_inject_fault('vacuum_ao_post_cleanup_end', 'suspend', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
2: SELECT gp_inject_fault('vacuum_worker_changed', 'reset', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
2: SELECT gp_wait_until_triggered_fault('vacuum_ao_post_cleanup_end', 1, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- The previous collected num_dead_tuples in compact phase is zero.
select gp_segment_id, relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from gp_stat_progress_vacuum where gp_segment_id > -1;
select relid::regclass as relname, phase, heap_blks_total, heap_blks_scanned, heap_blks_vacuumed, index_vacuum_count, max_dead_tuples, num_dead_tuples from gp_stat_progress_vacuum_summary;

2: SELECT gp_inject_fault('vacuum_ao_post_cleanup_end', 'reset', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

1<:

-- restore environment
1: reset debug_appendonly_print_compaction;

2: SELECT pg_ctl_start(datadir, port) FROM gp_segment_configuration WHERE role = 'm' AND content = 1;
2: SELECT wait_until_all_segments_synchronized();

-- Cleanup
SELECT gp_inject_fault_infinite('all', 'reset', dbid) FROM gp_segment_configuration;
reset Debug_appendonly_print_compaction;
reset default_table_access_method;
