-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE; 
CREATE SCHEMA QUERY_METRICS;
SET SEARCH_PATH=QUERY_METRICS;
CREATE EXTERNAL WEB TABLE __gp_localid
(
    localid    int
)
EXECUTE E'echo $GP_SEGMENT_ID' FORMAT 'TEXT';
GRANT SELECT ON TABLE __gp_localid TO public;
CREATE EXTERNAL WEB TABLE __gp_masterid
(
    masterid    int
)
EXECUTE E'echo $GP_SEGMENT_ID' ON MASTER FORMAT 'TEXT';
GRANT SELECT ON TABLE __gp_masterid TO public;
CREATE FUNCTION gp_instrument_shmem_summary_f()
RETURNS SETOF RECORD
AS '$libdir/gp_instrument_shmem', 'gp_instrument_shmem_summary'
LANGUAGE C IMMUTABLE;
GRANT EXECUTE ON FUNCTION gp_instrument_shmem_summary_f() TO public;
CREATE VIEW gp_instrument_shmem_summary AS
WITH all_entries AS (
  SELECT C.*
    FROM __gp_localid, gp_instrument_shmem_summary_f() as C (
      segid int, num_free bigint, num_used bigint
    )
  UNION ALL
  SELECT C.*
    FROM __gp_masterid, gp_instrument_shmem_summary_f() as C (
      segid int, num_free bigint, num_used bigint
    ))
SELECT segid, num_free, num_used
FROM all_entries
ORDER BY segid;
GRANT SELECT ON gp_instrument_shmem_summary TO public;
SET OPTIMIZER=OFF;
-- end_ignore
select count(*) from pg_stat_activity;
set gp_enable_query_metrics=off;
SELECT * FROM gp_instrument_shmem_summary WHERE segid = -1;
SELECT * FROM gp_instrument_shmem_summary WHERE segid = 0;
SELECT * FROM gp_instrument_shmem_summary WHERE segid = 1;
set gp_enable_query_metrics=on;
-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE; 
-- end_ignore
