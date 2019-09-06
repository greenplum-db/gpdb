-- This test is to verify the order of reusing idle gangs.
--
-- For example:
-- In the same session,
-- query 1 has 3 slices and it creates gang B, gang C and gang D.
-- query 2 has 2 slices, we hope it reuses gang B and gang C instead of other
-- cases like gang D and gang C.
--
-- In this way, the two queries can have the same send-receive port pair. It's
-- useful in platform like Azure. Because Azure limits the number of different
-- send-receive port pairs (AKA flow) in a certain time period.

-- To verify the order we show the gang id in EXPLAIN ANALYZE output when
-- gp_log_gang is 'debug', turn on this output.
set gp_log_gang to 'debug';
set gp_cached_segworkers_threshold to 10;
set gp_vmem_idle_resource_timeout to '60s';
set optimizer_enable_motion_broadcast to off;
set optimizer_force_multistage_agg to on;

create table test_gang_reuse_t1 (c1 int, c2 int);

-- this query will create 3 reader gangs with ids C, D and E, we expect they
-- will always be reused in the same order
explain analyze select count(*) from test_gang_reuse_t1 a
  join test_gang_reuse_t1 b using (c2)
  join test_gang_reuse_t1 c using (c2)
;

-- so in this query the gangs C and D should be used
explain analyze select count(*) from test_gang_reuse_t1 a
  join test_gang_reuse_t1 b using (c2)
;

-- so in this query the gangs C, D and E should be used
explain analyze select count(*) from test_gang_reuse_t1 a
  join test_gang_reuse_t1 b using (c2)
  join test_gang_reuse_t1 c using (c2)
;

-- so in this query the gangs C and D should be used
explain analyze select count(*) from test_gang_reuse_t1 a
  join test_gang_reuse_t1 b using (c2)
;

reset optimizer_force_multistage_agg;
