-- Test that gp_switch_wal() returns back WAL segment filenames
-- constructed on the individual segments so that their timeline ids are
-- used instead of each result having the same timeline id.

-- timeline ids prior to failover/failback should all be 1 due to the
-- test requirement of having a fresh gpdemo cluster with mirrors
SELECT gp_segment_id, substring(pg_walfile_name, 1, 8) FROM gp_switch_wal() ORDER BY gp_segment_id;

-- stop a primary in order to trigger a mirror promotion
SELECT pg_ctl((SELECT datadir FROM gp_segment_configuration WHERE role = 'p' AND content = 1), 'stop');

-- trigger failover
select gp_request_fts_probe_scan();

-- wait for content 1 (earlier mirror, now primary) to finish the promotion
0U: SELECT 1;

-- recover the failed primary as new mirror
!\retcode gprecoverseg -a --no-progress;

-- loop while segments come in sync
SELECT wait_until_all_segments_synchronized();

-- rebalance back
!\retcode gprecoverseg -ar --no-progress;

-- loop while segments come in sync
SELECT wait_until_all_segments_synchronized();

-- test that gp_switch_wal() uses the segment-specific timeline id to construct each WAL filename
SELECT gp_segment_id, substring(pg_walfile_name, 1, 8) FROM gp_switch_wal() ORDER BY gp_segment_id;

-- test simple gp_switch_wal() error scenarios
SELECT gp_switch_wal() FROM gp_dist_random('gp_id');

CREATE TABLE this_ctas_should_fail AS SELECT gp_segment_id AS contentid, pg_switch_wal, pg_walfile_name FROM gp_switch_wal();

CREATE ROLE switch_wal_error_role;
SET ROLE TO switch_wal_error_role;
SELECT * FROM gp_switch_wal();
RESET ROLE;
DROP ROLE switch_wal_error_role;
