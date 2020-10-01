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

CREATE TABLE a (id int, c char) DISTRIBUTED BY (id);
INSERT INTO a SELECT *, 'a' FROM generate_series(1, 50);
SET OPTIMIZER=OFF;
ANALYZE a;
-- end_ignore

-- test 1: pg_terminate_backend
-- only this query in instrument slots, expected 1
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

CREATE TABLE foo AS SELECT i a, i b FROM generate_series(1, 10) i;

-- this query will be terminated by 'test pg_terminate_backend'
1&:EXPLAIN ANALYZE CREATE TEMP TABLE t1 AS SELECT count(*) FROM QUERY_METRICS.foo WHERE pg_sleep(200) IS NULL;
-- terminate above query
SELECT pg_terminate_backend(pid, 'test pg_terminate_backend')
FROM pg_stat_activity WHERE query LIKE 'EXPLAIN ANALYZE CREATE TEMP TABLE t1 AS SELECT%' ORDER BY pid LIMIT 1;
-- start_ignore
1<:
1q:
-- end_ignore

-- query backend to ensure no PANIC on postmaster and wait cleanup done
SELECT count(*) FROM foo, pg_sleep(2);

-- test 2: pg_cancel_backend
-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- this query will be cancelled by 'test pg_cancel_backend'
2&:EXPLAIN ANALYZE CREATE TEMP TABLE t2 AS SELECT count(*) FROM QUERY_METRICS.foo WHERE pg_sleep(200) IS NULL;
-- cancel above query
SELECT pg_cancel_backend(pid, 'test pg_cancel_backend')
FROM pg_stat_activity WHERE query LIKE 'EXPLAIN ANALYZE CREATE TEMP TABLE t2 AS SELECT%' ORDER BY pid LIMIT 1;
-- start_ignore
2<:
2q:
-- end_ignore

-- query backend to ensure no PANIC on postmaster and wait cleanup done
SELECT count(*) FROM foo, pg_sleep(2);

-- test 3: DML should expose plan_node_id for whole plan tree
-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- this query will be cancelled by 'test pg_cancel_backend'
3&:SET OPTIMIZER TO off;EXPLAIN ANALYZE INSERT INTO QUERY_METRICS.a SELECT *, pg_sleep(20) FROM generate_series(1,10);

-- validate plan nodes exist in instrument solts
SELECT count(*) FROM pg_sleep(1);
SELECT ro, CASE WHEN max(nid) > 2 THEN 'ok' ELSE 'wrong' END isok FROM (
  SELECT CASE WHEN segid >= 0 THEN 's' ELSE 'm' END ro, nid FROM gp_instrument_shmem_detail
  WHERE ssid <> (SELECT setting FROM pg_settings WHERE name = 'gp_session_id')::int AND nid > 0
) dt
GROUP BY (ro) ORDER BY ro;
-- validate no different tmid across segments
SELECT count(*) FROM (SELECT DISTINCT tmid FROM gp_instrument_shmem_detail) t;
-- cancel the query
SELECT pg_cancel_backend(pid, 'test DML')
FROM pg_stat_activity WHERE query LIKE 'SET OPTIMIZER TO off;EXPLAIN ANALYZE INSERT INTO QUERY_METRICS.a SELECT%' ORDER BY pid LIMIT 1;

-- start_ignore
3<:
3q:
-- end_ignore

-- query backend to ensure no PANIC on postmaster and wait cleanup done
SELECT count(*) FROM foo, pg_sleep(2);

-- test 4: Merge Append should expose plan_node_id for whole plan tree
CREATE TABLE QUERY_METRICS.mergeappend_test (a int, b int, x int) DISTRIBUTED BY (a,b);
INSERT INTO QUERY_METRICS.mergeappend_test SELECT g/100, g/100, g FROM generate_series(1, 500) g;
ANALYZE QUERY_METRICS.mergeappend_test;

-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- this query will be cancelled by 'test pg_cancel_backend'
4&:SET OPTIMIZER TO off;SELECT a, b, array_dims(array_agg(x)) FROM QUERY_METRICS.mergeappend_test r GROUP BY a, b
UNION ALL
SELECT NULL, NULL, array_dims(array_agg(x)) FROM QUERY_METRICS.mergeappend_test r, pg_sleep(200)
ORDER BY 1,2;

-- validate plan nodes exist in instrument solts
SELECT count(*) FROM pg_sleep(1);
SELECT ro, CASE WHEN max(nid) > 5 THEN 'ok' ELSE 'wrong' END isok FROM (
  SELECT CASE WHEN segid >= 0 THEN 's' ELSE 'm' END ro, nid FROM gp_instrument_shmem_detail
  WHERE ssid <> (SELECT setting FROM pg_settings WHERE name = 'gp_session_id')::int AND nid > 0
) dt
GROUP BY (ro) ORDER BY ro;
-- validate no different tmid across segments
SELECT count(*) FROM (SELECT DISTINCT tmid FROM gp_instrument_shmem_detail) t;
-- cancel the query
SELECT pg_cancel_backend(pid, 'test MergeAppend')
FROM pg_stat_activity WHERE query LIKE 'SET OPTIMIZER TO off;SELECT a, b, array_dims(array_agg(x)) FROM QUERY_METRICS.mergeappend_test%' ORDER BY pid LIMIT 1;

-- start_ignore
4<:
4q:
-- end_ignore

-- query backend to ensure no PANIC on postmaster and wait cleanup done
SELECT count(*) FROM foo, pg_sleep(2);

-- test 5: entrydb
-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- this query will be cancelled by 'test pg_cancel_backend'
5&:SET OPTIMIZER TO off;EXPLAIN ANALYZE INSERT INTO QUERY_METRICS.a(id) SELECT oid FROM pg_class, pg_sleep(200);

-- validate QD and entrydb have instrumentations on each backend process
SELECT count(*) FROM pg_sleep(1);
SELECT count(DISTINCT pid) FROM gp_instrument_shmem_detail
WHERE ssid <> (SELECT setting FROM pg_settings WHERE name = 'gp_session_id')::int
 AND segid < 0 AND nid >= 4;
-- validate no different tmid across segments
SELECT count(*) FROM (SELECT DISTINCT tmid FROM gp_instrument_shmem_detail) t;
-- cancel the query
SELECT pg_cancel_backend(pid, 'test entrydb')
FROM pg_stat_activity WHERE query LIKE 'SET OPTIMIZER TO off;EXPLAIN ANALYZE INSERT INTO QUERY_METRICS.a(id) SELECT%' ORDER BY pid LIMIT 1;

-- start_ignore
5<:
5q:
-- end_ignore

-- query backend to ensure no PANIC on postmaster and wait cleanup done
SELECT count(*) FROM foo, pg_sleep(2);

-- Expected result is 1 row, means only current query in instrument slots,
-- If more than one row returned, means previous test has leaked slots.
SELECT count(*) FROM (SELECT 1 FROM gp_instrument_shmem_detail GROUP BY ssid, ccnt) t;

-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE;
-- end_ignore
