-- start_ignore
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
DROP TABLE IF EXISTS test_src_tbl;
DROP TABLE IF EXISTS test_hashagg_on;
DROP TABLE IF EXISTS test_hashagg_off;
-- end_ignore

-- Test Orca properly removes duplicates in DQA
-- (https://github.com/greenplum-db/gpdb/issues/14993)

-- GPDB_12_MERGE_FEATURE_NOT_SUPPORTED: After streaming hash aggregates are
-- supported then add a fault injection for 'force_hashagg_stream_hashtable'.
-- Until then this test doesn't actually test spilling.

CREATE TABLE test_src_tbl AS
WITH cte1 AS (
    SELECT field5 from generate_series(1,1000) field5
)
SELECT field5 % 100 AS a, field5 % 100  + 1 AS b
FROM cte1 DISTRIBUTED BY (a);
ANALYZE test_src_tbl;


-- Use isolation2 framework to force a streaming hash aggregate to clear the
-- hash table and stream tuples to next stage aggregate. This is to simulate
-- hash table spills after 100 tuples inserted any segment.
SELECT gp_inject_fault('force_hashagg_stream_hashtable', 'skip', '', '', '', 100, 100, 0, dbid) FROM gp_segment_configuration WHERE role='p';
CREATE TABLE test_hashagg_on AS
SELECT a, COUNT(DISTINCT b) AS b FROM test_src_tbl GROUP BY a;
EXPLAIN (costs off) SELECT a, COUNT(DISTINCT b) AS b FROM test_src_tbl GROUP BY a;

-- Compare results against a group aggregate plan.
set optimizer_enable_hashagg=off;
CREATE TABLE test_hashagg_off AS
SELECT a, COUNT(DISTINCT b) AS b FROM test_src_tbl GROUP BY a;
EXPLAIN (costs off) SELECT a, COUNT(DISTINCT b) AS b FROM test_src_tbl GROUP BY a;

-- Results should match
SELECT (n_total=n_matches) AS match FROM (
SELECT COUNT(*) n_total, SUM(CASE WHEN t1.b = t2.b THEN 1 ELSE 0 END) n_matches
FROM test_hashagg_on t1
JOIN test_hashagg_off t2 ON t1.a = t2.a) t;


-- start_ignore
SELECT gp_inject_fault('force_hashagg_stream_hashtable', 'status', '', '', '', 100, 100, 0, dbid) FROM gp_segment_configuration WHERE role='p';
SELECT gp_inject_fault('force_hashagg_stream_hashtable', 'reset', '', '', '', 100, 100, 0, dbid) FROM gp_segment_configuration WHERE role='p';
RESET ALL;
-- end_ignore
