-- create a resource group when gp_resource_manager is queue
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH
(concurrency=1, cpu_rate_limit=20, memory_limit=60, memory_shared_quota=0, memory_spill_ratio=10);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

--
-- increase concurrency after pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;

11:SET ROLE role_concurrency_test;
11:BEGIN;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21&:BEGIN;
22&:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;

SELECT rsgname,waiting_reason,current_query FROM pg_stat_activity;

11:END;
11q:

SELECT rsgname,waiting_reason,current_query FROM pg_stat_activity;

21<:
22<:
21:END;
22:END;
21q:
22q:

SELECT rsgname,waiting_reason,current_query FROM pg_stat_activity;

--
-- increase concurrency before pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;

11:SET ROLE role_concurrency_test;
11:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21&:BEGIN;
22&:BEGIN;

SELECT rsgname,waiting_reason,current_query FROM pg_stat_activity;

11:END;
11q:

SELECT rsgname,waiting_reason,current_query FROM pg_stat_activity;

21<:
22<:
21:END;
22:END;
21q:
22q:

SELECT rsgname,waiting_reason,current_query FROM pg_stat_activity;

-- cleanup
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
