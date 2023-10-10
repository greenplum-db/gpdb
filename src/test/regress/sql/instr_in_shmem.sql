-- Test instrument in shmem does not break EXPLAIN
-- and EXPLAIN ANALYZE. Also instrumentation slots
-- are correctly recycled.
-- This test can not run in parallel with other tests.

-- default value
SHOW GP_ENABLE_QUERY_METRICS;
SELECT 1;

SHOW GP_INSTRUMENT_SHMEM_SIZE;
SELECT 1;

-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE; 
-- end_ignore
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
EXECUTE E'echo $GP_SEGMENT_ID' ON COORDINATOR FORMAT 'TEXT';
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

CREATE FUNCTION gp_instrument_shmem_detail_f()
RETURNS SETOF RECORD
AS '$libdir/gp_instrument_shmem', 'gp_instrument_shmem_detail'
LANGUAGE C IMMUTABLE;
GRANT EXECUTE ON FUNCTION gp_instrument_shmem_detail_f() TO public;

CREATE VIEW gp_instrument_shmem_detail AS
WITH all_entries AS (
  SELECT C.*
    FROM __gp_localid, gp_instrument_shmem_detail_f() as C (
      tmid int4,ssid int4,ccnt int2,segid int2,pid int4
      ,nid int2,tuplecount int8,nloops int8,ntuples int8
    )
  UNION ALL
  SELECT C.*
    FROM __gp_masterid, gp_instrument_shmem_detail_f() as C (
      tmid int4,ssid int4,ccnt int2,segid int2,pid int4
      ,nid int2,tuplecount int8,nloops int8,ntuples int8
    ))
SELECT tmid, ssid, ccnt,segid, pid, nid, tuplecount, nloops, ntuples
FROM all_entries
ORDER BY segid;
GRANT SELECT ON gp_instrument_shmem_detail TO public;

CREATE TABLE a (id int) DISTRIBUTED BY (id);
INSERT INTO a SELECT * FROM generate_series(1, 50);
SET OPTIMIZER=OFF;
ANALYZE a;

-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- regression to EXPLAN ANALYZE
EXPLAIN ANALYZE SELECT 1/0;
EXPLAIN ANALYZE SELECT count(*) FROM a where id < (1/(select count(*) where 1=0));
EXPLAIN ANALYZE SELECT count(*) FROM a a1, a a2, a a3;
EXPLAIN SELECT 1/0;
EXPLAIN SELECT count(*) FROM a where id < (1/(select count(*) where 1=0));
EXPLAIN SELECT count(*) FROM a a1, a a2, a a3;

-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

DROP SCHEMA QUERY_METRICS CASCADE;
