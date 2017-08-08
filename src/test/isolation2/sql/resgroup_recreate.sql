-- start_ignore
DROP ROLE IF EXISTS r1;
DROP RESOURCE GROUP rg1;
-- end_ignore

CREATE RESOURCE GROUP rg1 WITH (concurrency=2, cpu_rate_limit=10,
	memory_limit=50, memory_shared_quota=0);
CREATE ROLE r1 RESOURCE GROUP rg1;

1: SET ROLE r1;
1: BEGIN;
1: END;

ALTER ROLE r1 RESOURCE GROUP none;
DROP RESOURCE GROUP rg1;
CREATE RESOURCE GROUP rg1 WITH (concurrency=2, cpu_rate_limit=10,
	memory_limit=50, memory_shared_quota=0);
ALTER ROLE r1 RESOURCE GROUP rg1;

1: BEGIN;
1: END;

DROP ROLE r1;
DROP RESOURCE GROUP rg1;
