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
CREATE TABLE results1 (segid int, free bigint, used bigint);
CREATE TABLE results2 (segid int, free bigint, used bigint);

set gp_enable_query_metrics=off;
INSERT INTO expected SELECT * FROM gp_instrument_shmem_summary WHERE segid = -1;
INSERT INTO expected SELECT * FROM gp_instrument_shmem_summary WHERE segid = 0;
INSERT INTO expected SELECT * FROM gp_instrument_shmem_summary WHERE segid = 1;
set gp_enable_query_metrics=on;
-- end_ignore

create table foo as select i a, i b from generate_series(1, 10) i;

-- expect this query terminated by 'test pg_terminate_backend'
1&:explain analyze create temp table t1 as select count(*) from foo where pg_sleep(20) is null;
-- extract the pid for the previous query
SELECT pg_terminate_backend(procpid,'test pg_terminate_backend')
FROM pg_stat_activity WHERE current_query like 'explain analyze create temp table t1 as select%' ORDER BY procpid LIMIT 1;
1<:
1q:

-- query backend to ensure no PANIC on postmaster
select count(*) from foo;

-- start_ignore
set gp_enable_query_metrics=off;
INSERT INTO results1 SELECT * FROM gp_instrument_shmem_summary WHERE segid = -1;
INSERT INTO results1 SELECT * FROM gp_instrument_shmem_summary WHERE segid = 0;
INSERT INTO results1 SELECT * FROM gp_instrument_shmem_summary WHERE segid = 1;
set gp_enable_query_metrics=on;
-- end_ignore

-- ensure no instrument slot leak by terminate
SELECT * FROM expected
EXCEPT
SELECT * FROM results1;

SELECT * FROM results1
EXCEPT
SELECT * FROM expected;

-- expect this query cancelled by 'test pg_cancel_backend'
2&:explain analyze create temp table t2 as select count(*) from foo where pg_sleep(20) is null;
-- extract the pid for the previous query
SELECT pg_cancel_backend(procpid,'test pg_cancel_backend')
FROM pg_stat_activity WHERE current_query like 'explain analyze create temp table t2 as select%' ORDER BY procpid LIMIT 1;
2<:
2q:

-- query backend to ensure no PANIC on postmaster
select count(*) from foo;

-- start_ignore
set gp_enable_query_metrics=off;
INSERT INTO results2 SELECT * FROM gp_instrument_shmem_summary WHERE segid = -1;
INSERT INTO results2 SELECT * FROM gp_instrument_shmem_summary WHERE segid = 0;
INSERT INTO results2 SELECT * FROM gp_instrument_shmem_summary WHERE segid = 1;
set gp_enable_query_metrics=on;
-- end_ignore

-- ensure no instrument slot leak by cancel
SELECT * FROM results1
EXCEPT
SELECT * FROM results2;

SELECT * FROM results2
EXCEPT
SELECT * FROM results1;

-- start_ignore
DROP SCHEMA IF EXISTS QUERY_METRICS CASCADE;
-- end_ignore
