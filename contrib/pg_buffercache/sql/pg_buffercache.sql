CREATE EXTENSION pg_buffercache;

select count(*) = (select setting::bigint
                   from pg_settings
                   where name = 'shared_buffers')
from pg_buffercache;

select buffers_used + buffers_unused > 0,
        buffers_dirty <= buffers_used,
        buffers_pinned <= buffers_used
from pg_buffercache_summary();

SELECT count(*) > 0 FROM pg_buffercache_usage_counts() WHERE buffers >= 0;

-- Test GPDB functions/views
SELECT count(*) = (select setting::bigint
                   from pg_settings
                   where name = 'shared_buffers') *
                   (select count(*) from gp_segment_configuration where role='p')
                   as buffers
FROM gp_buffercache;

SELECT buffers_used + buffers_unused > 0,
        buffers_dirty <= buffers_used,
        buffers_pinned <= buffers_used
FROM gp_buffercache_summary;

SELECT buffers_used + buffers_unused > 0,
        buffers_dirty <= buffers_used,
        buffers_pinned <= buffers_used
FROM gp_buffercache_summary_aggregated;

SELECT count(*) > 0 FROM gp_buffercache_usage_counts WHERE buffers >= 0;

SELECT count(*) > 0 FROM gp_buffercache_usage_counts_aggregated WHERE buffers >= 0;

-- Check that the functions / views can't be accessed by default.
CREATE ROLE buffercache_test;
SET ROLE buffercache_test;
SELECT * FROM pg_buffercache;
SELECT * FROM pg_buffercache_pages() AS p (wrong int);
SELECT * FROM pg_buffercache_summary();
SELECT * FROM pg_buffercache_usage_counts();
-- GPDB
SELECT * FROM pg_buffercache_summary;
SELECT * FROM pg_buffercache_usage_counts;
SELECT * FROM gp_buffercache;
SELECT * FROM gp_buffercache_summary;
SELECT * FROM gp_buffercache_usage_counts;
SELECT * FROM gp_buffercache_summary_aggregated;
SELECT * FROM gp_buffercache_usage_counts_aggregated;
RESET ROLE;

-- Check that pg_monitor is allowed to query view / function
SET ROLE pg_monitor;
SELECT count(*) > 0 FROM pg_buffercache;
SELECT buffers_used + buffers_unused > 0 FROM pg_buffercache_summary();
SELECT count(*) > 0 FROM pg_buffercache_usage_counts();

-- GPDB
SELECT count(*) > 0 FROM pg_buffercache_summary;
SELECT count(*) > 0 FROM pg_buffercache_usage_counts;
SELECT count(*) > 0 FROM gp_buffercache;
SELECT buffers_used + buffers_unused > 0 FROM gp_buffercache_summary;
SELECT buffers_used + buffers_unused > 0 FROM gp_buffercache_summary_aggregated;
SELECT count(*) > 0 FROM gp_buffercache_usage_counts;
SELECT count(*) > 0 FROM gp_buffercache_usage_counts_aggregated;
RESET ROLE;

DROP ROLE buffercache_test;
