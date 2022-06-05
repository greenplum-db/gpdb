-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_subtrx_overflow" to load this file. \quit

CREATE OR REPLACE FUNCTION gp_find_subtx_overflowed_pids()
RETURNS text
AS '$libdir/gp_subtrx_overflow'
LANGUAGE C;

-- Dispatch and aggregate all pids from coordinator and segments
CREATE VIEW gp_subtrx_backend(segid, pids) AS
  select -1, gp_find_subtx_overflowed_pids()
UNION ALL
  select gp_segment_id, gp_find_subtx_overflowed_pids() from gp_dist_random('gp_id');