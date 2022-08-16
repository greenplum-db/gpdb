-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_subtransaction_overflow" to load this file. \quit

CREATE OR REPLACE FUNCTION gp_get_suboverflowed_backends()
RETURNS int[]
AS '$libdir/gp_subtransaction_overflow'
LANGUAGE C;

-- Dispatch and aggregate all backends from coordinator and segments
CREATE VIEW gp_suboverflowed_backend(segid, pids) AS
  SELECT -1, gp_get_suboverflowed_backends()
UNION ALL
  SELECT gp_segment_id, gp_get_suboverflowed_backends() FROM gp_dist_random('gp_id') order by 1;
