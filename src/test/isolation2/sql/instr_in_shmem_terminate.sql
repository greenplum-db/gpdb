-- start_ignore
-- Isolation test for instrumentation in shmem
-- One session executing a query then another session
-- try to cancel/terminate the query, instrumentation
-- slots in shmem should be recycled correctly.

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
GRANT SELECT ON gp_instrument_shmem_details TO public;

CREATE TABLE a (id int) DISTRIBUTED BY (id);
INSERT INTO a SELECT * FROM generate_series(1, 50);
SET OPTIMIZER=OFF;
ANALYZE a;
-- end_ignore

-- only this query in instrument slots, expected 1
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

CREATE TABLE foo AS SELECT i a, i b FROM generate_series(1, 10) i;

-- expect this query terminated by 'test pg_terminate_backend'
1&:EXPLAIN ANALYZE CREATE TEMP TABLE t1 AS SELECT count(*) FROM QUERY_METRICS.foo WHERE pg_sleep(20) IS NULL;
-- extract the pid for the previous query
SELECT pg_terminate_backend(procpid, 'test pg_terminate_backend')
FROM pg_stat_activity WHERE current_query LIKE 'EXPLAIN ANALYZE CREATE TEMP TABLE t1 AS SELECT%' ORDER BY procpid LIMIT 1;
1<:
1q:

-- query backend to ensure no PANIC on postmaster
SELECT count(*) FROM foo;

-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- expect this query cancelled by 'test pg_cancel_backend'
2&:EXPLAIN ANALYZE CREATE TEMP TABLE t2 AS SELECT count(*) FROM QUERY_METRICS.foo WHERE pg_sleep(20) IS NULL;
-- extract the pid for the previous query
SELECT pg_cancel_backend(procpid, 'test pg_cancel_backend')
FROM pg_stat_activity WHERE current_query LIKE 'EXPLAIN ANALYZE CREATE TEMP TABLE t2 AS SELECT%' ORDER BY procpid LIMIT 1;
2<:
2q:

-- query backend to ensure no PANIC on postmaster
SELECT count(*) FROM foo;

-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE;
-- end_ignore
