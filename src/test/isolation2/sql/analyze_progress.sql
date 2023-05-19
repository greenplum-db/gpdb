-- Test gp_stat_progress_analyze_summary
-- setup hash distributed table
CREATE TABLE t_analyze_part (a INT, b INT) DISTRIBUTED BY (a);
INSERT INTO t_analyze_part SELECT i, i FROM generate_series(1, 100000) i;

-- Suspend analyze after scanning 20 blocks on each segment
SELECT gp_inject_fault('analyze_block', 'suspend', '', '', '', 20, 20, 0, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- session 1: analyze the table
1&: ANALYZE t_analyze_part;
SELECT gp_wait_until_triggered_fault('analyze_block', 1, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- session 2: query pg_stat_progress_analyze while the analyze is running, the view should indicate 60 blocks have been scanned as aggregated progress of 3 segments
2: SELECT pid IS NOT NULL as has_pid, datname, relid::regclass, phase, sample_blks_total, sample_blks_scanned FROM gp_stat_progress_analyze_summary;

-- Reset fault injector
SELECT gp_inject_fault('analyze_block', 'reset', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
1<:

-- teardown
DROP TABLE t_analyze_part;

-- setup replicated table
CREATE TABLE t_analyze_repl (a INT, b INT) DISTRIBUTED REPLICATED;
INSERT INTO t_analyze_repl SELECT i, i FROM generate_series(1, 100000) i;

-- Suspend analyze after scanning 20 blocks on each segment
SELECT gp_inject_fault('analyze_block', 'suspend', '', '', '', 20, 20, 0, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- session 1: analyze the table
1&: ANALYZE t_analyze_repl;
SELECT gp_wait_until_triggered_fault('analyze_block', 1, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- session 2: query pg_stat_progress_analyze while the analyze is running, the view should indicate 20 blocks have been scanned as average progress of 3 segments
2: SELECT pid IS NOT NULL as has_pid, datname, relid::regclass, phase, sample_blks_total, sample_blks_scanned FROM gp_stat_progress_analyze_summary;

-- Reset fault injector
SELECT gp_inject_fault('analyze_block', 'reset', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
1<:

-- teardown
DROP TABLE t_analyze_repl;

