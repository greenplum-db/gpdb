CREATE TABLE test_zero_workmem(c int);
INSERT INTO test_zero_workmem SELECT generate_series(1, 100);

--This test intends to build a situation that query_mem = 0
--and verify under such condition work_mem will be used.
--It is designed to pass the test on concourse(3G RAM) and dev docker.
CREATE RESOURCE GROUP rg_zero_workmem WITH
(concurrency=25, cpu_rate_limit=10, memory_limit=1, memory_shared_quota=0, memory_spill_ratio=1);

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
