/* contrib/pg_buffercache/pg_buffercache--1.4--1.4.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pg_buffercache UPDATE TO '1.4.1'" to load this file. \quit


-- Wrap the functions in a view for convenience.
CREATE VIEW pg_buffercache_summary AS
	SELECT * FROM pg_buffercache_summary();

CREATE VIEW pg_buffercache_usage_counts AS
	SELECT * FROM pg_buffercache_usage_counts();

-- Create MPP views to gather results from coordinator and all segments.
CREATE VIEW gp_buffercache AS
	SELECT gp_execution_segment() AS gp_segment_id, *
	FROM gp_dist_random('pg_buffercache')
	UNION ALL
	SELECT -1 AS gp_segment_id, *
  FROM pg_buffercache
  ORDER BY 1,2;

CREATE VIEW gp_buffercache_summary AS
	SELECT gp_execution_segment() AS gp_segment_id, *
	FROM gp_dist_random('pg_buffercache_summary')
	UNION ALL
	SELECT -1 AS gp_segment_id, *
	FROM pg_buffercache_summary
	ORDER BY 1;

CREATE VIEW gp_buffercache_usage_counts AS
	SELECT gp_execution_segment() AS gp_segment_id, *
	FROM gp_dist_random('pg_buffercache_usage_counts')
	UNION ALL
	SELECT -1 AS gp_segment_id, *
	FROM pg_buffercache_usage_counts
	ORDER BY 1,2;

-- Create aggregate views.
CREATE VIEW gp_buffercache_summary_aggregated AS
  SELECT
    sum(buffers_used) AS buffers_used,
    sum(buffers_unused) AS buffers_unused,
    sum(buffers_dirty) AS buffers_dirty,
    sum(buffers_pinned) AS buffers_pinned,
    avg(usagecount_avg) AS usagecount_avg
  FROM gp_buffercache_summary;

CREATE VIEW gp_buffercache_usage_counts_aggregated AS
	SELECT
    sum(usage_count) AS usage_count,
    sum(buffers) AS buffers,
    sum(dirty) AS dirty,
    sum(pinned) AS pinned
	FROM gp_buffercache_usage_counts;


REVOKE ALL ON pg_buffercache_summary FROM PUBLIC;
GRANT SELECT ON pg_buffercache_summary TO pg_monitor;

REVOKE ALL ON pg_buffercache_usage_counts FROM PUBLIC;
GRANT SELECT ON pg_buffercache_usage_counts TO pg_monitor;

REVOKE ALL ON gp_buffercache FROM PUBLIC;
GRANT SELECT ON gp_buffercache TO pg_monitor;

REVOKE ALL ON gp_buffercache_summary FROM PUBLIC;
GRANT SELECT ON gp_buffercache_summary TO pg_monitor;

REVOKE ALL ON gp_buffercache_usage_counts FROM PUBLIC;
GRANT SELECT ON gp_buffercache_usage_counts TO pg_monitor;

REVOKE ALL ON gp_buffercache_summary_aggregated FROM PUBLIC;
GRANT SELECT ON gp_buffercache_summary_aggregated TO pg_monitor;

REVOKE ALL ON gp_buffercache_usage_counts_aggregated FROM PUBLIC;
GRANT SELECT ON gp_buffercache_usage_counts_aggregated TO pg_monitor;
