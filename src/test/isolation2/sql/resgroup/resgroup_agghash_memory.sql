-- When an agghash operator begins to spill it needs 16KB memory as meta data
-- for each batch file, when there are many batch files there might not be
-- enough operator memory for all of them, in resource queue mode the query
-- will fail with an error like below:
--
--    ERROR:  insufficient memory reserved for statement
--
-- In resource group we allow this overuse as the shared memory is designed to
-- serve this kind of overuse.

--start_ignore
DROP TABLE IF EXISTS t1_agghash_mem_test;
DROP ROLE r1_agghash_mem_test;
DROP RESOURCE GROUP rg1_agghash_mem_test;
--end_ignore

SET optimizer TO off;

CREATE TABLE t1_agghash_mem_test (c1 int, c2 text) DISTRIBUTED BY (c2);
INSERT INTO t1_agghash_mem_test SELECT i, i::text FROM generate_series(1,100000) i;

-- we must ensure spill to be small enough but still > 0.
-- - rg1's memory quota is 682 * 1% = 6;
-- - per-xact quota is 6/3=2;
-- - spill memory is 2 * 60% = 1;
CREATE RESOURCE GROUP rg1_agghash_mem_test
  WITH (cpu_rate_limit=10, memory_limit=1, memory_shared_quota=0,
        concurrency=3, memory_spill_ratio=60);

CREATE ROLE r1_agghash_mem_test RESOURCE GROUP rg1_agghash_mem_test;
GRANT ALL ON t1_agghash_mem_test TO r1_agghash_mem_test;

SET gp_resgroup_memory_policy TO none;
SET ROLE TO r1_agghash_mem_test;
WITH a AS (
	SELECT DISTINCT c2 FROM t1_agghash_mem_test INTERSECT
	SELECT DISTINCT c2 FROM t1_agghash_mem_test
) SELECT count(*) FROM a;
RESET role;

SET gp_resgroup_memory_policy TO auto;
SET ROLE TO r1_agghash_mem_test;
WITH a AS (
	SELECT DISTINCT c2 FROM t1_agghash_mem_test INTERSECT
	SELECT DISTINCT c2 FROM t1_agghash_mem_test
) SELECT count(*) FROM a;
RESET role;

SET gp_resgroup_memory_policy TO eager_free;
SET ROLE TO r1_agghash_mem_test;
WITH a AS (
	SELECT DISTINCT c2 FROM t1_agghash_mem_test INTERSECT
	SELECT DISTINCT c2 FROM t1_agghash_mem_test
) SELECT count(*) FROM a;
RESET role;

DROP TABLE IF EXISTS t1_agghash_mem_test;
DROP ROLE r1_agghash_mem_test;
DROP RESOURCE GROUP rg1_agghash_mem_test;
