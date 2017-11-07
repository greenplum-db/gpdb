-- default value
SHOW GP_ENABLE_QUERY_METRICS;
SELECT 1;

-- turn off
SET GP_ENABLE_QUERY_METRICS=OFF;
SHOW GP_ENABLE_QUERY_METRICS;
SELECT 1;

-- turn on
SET GP_ENABLE_QUERY_METRICS=ON;
SHOW GP_ENABLE_QUERY_METRICS;
SELECT 1;

-- turn off
SET GP_ENABLE_QUERY_METRICS=OFF;
SHOW GP_ENABLE_QUERY_METRICS;
SELECT 1;

-- reset
RESET GP_ENABLE_QUERY_METRICS;
SHOW GP_ENABLE_QUERY_METRICS;
SELECT 1;

-- default value
SHOW GP_QUERY_METRICS_PORT;
SELECT 1;

SHOW GP_MAX_SHMEM_INSTRUMENTS;
SELECT 1;

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

CREATE TABLE a (id int) DISTRIBUTED BY (id);
INSERT INTO a SELECT * FROM generate_series(1, 50);
SET OPTIMIZER=OFF;
ANALYZE a;

CREATE TABLE expected (segid int, free bigint, used bigint);
CREATE TABLE results (segid int, free bigint, used bigint);

set gp_enable_query_metrics=off;
INSERT INTO expected SELECT * FROM gp_instrument_shmem_summary WHERE segid = -1;
INSERT INTO expected SELECT * FROM gp_instrument_shmem_summary WHERE segid = 0;
INSERT INTO expected SELECT * FROM gp_instrument_shmem_summary WHERE segid = 1;
set gp_enable_query_metrics=on;
-- end_ignore

-- regression to EXPLAN ANALZE
EXPLAIN ANALYZE SELECT 1/0;
EXPLAIN ANALYZE SELECT count(*) FROM a where id < (1/(select count(*) where 1=0));
EXPLAIN ANALYZE SELECT count(*) FROM a a1, a a2, a a3;

-- start_ignore
set gp_enable_query_metrics=off;
INSERT INTO results SELECT * FROM gp_instrument_shmem_summary WHERE segid = -1;
INSERT INTO results SELECT * FROM gp_instrument_shmem_summary WHERE segid = 0;
INSERT INTO results SELECT * FROM gp_instrument_shmem_summary WHERE segid = 1;
set gp_enable_query_metrics=on;
-- end_ignore

SELECT * FROM expected
EXCEPT
SELECT * FROM results;

SELECT * FROM results
EXCEPT
SELECT * FROM expected;

-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE; 
-- end_ignore
