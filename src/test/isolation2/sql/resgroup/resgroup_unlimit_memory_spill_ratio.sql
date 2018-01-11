-- start_ignore
DROP RESOURCE GROUP rg_spill_test;
-- end_ignore

-- create
CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=60);
DROP RESOURCE GROUP rg_spill_test;

CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=0);
DROP RESOURCE GROUP rg_spill_test;

CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=100);
DROP RESOURCE GROUP rg_spill_test;

CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=-1);
DROP RESOURCE GROUP rg_spill_test;

CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=101);
DROP RESOURCE GROUP rg_spill_test;

-- alter
CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=20);

ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 60;
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 0;
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 100;
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO -1;
ALTER RESOURCE GROUP rg_spill_test SET MEMORY_SPILL_RATIO 101;

DROP RESOURCE GROUP rg_spill_test;

-- set GUC
CREATE RESOURCE GROUP rg_spill_test WITH
(concurrency=10, cpu_rate_limit=20, memory_limit=20, memory_shared_quota=50, memory_spill_ratio=20);

SET MEMORY_SPILL_RATIO TO 60;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

SET MEMORY_SPILL_RATIO TO 0;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

SET MEMORY_SPILL_RATIO TO 100;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

SET MEMORY_SPILL_RATIO TO -1;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

SET MEMORY_SPILL_RATIO TO 101;
SHOW MEMORY_SPILL_RATIO;
SELECT 1;

DROP RESOURCE GROUP rg_spill_test;

-- test case for query_mem=0
CREATE TABLE test_zero_workmem(c int);

--This test intends to build a situation that query_mem = 0
--and verify under such condition work_mem will be used.
CREATE RESOURCE GROUP rg_zero_workmem WITH
(concurrency=2, cpu_rate_limit=10, memory_limit=20, memory_shared_quota=20, memory_spill_ratio=0);

CREATE ROLE role_zero_workmem SUPERUSER RESOURCE GROUP rg_zero_workmem;
SET ROLE TO role_zero_workmem;

--test query that will use spi
ANALYZE test_zero_workmem;

--test normal DML
SELECT count(*) FROM test_zero_workmem;

--clean env
SET ROLE to gpadmin;
DROP TABLE test_zero_workmem;
DROP ROLE role_zero_workmem;
DROP RESOURCE GROUP rg_zero_workmem;
