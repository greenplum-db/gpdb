-- test1: cancel a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE OR REPLACE VIEW rg_concurrency_view AS
	SELECT waiting, waiting_reason, current_query, rsgname
	FROM pg_stat_activity
	WHERE rsgname='rg_concurrency_test';

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1:BEGIN;
2:SET ROLE role_concurrency_test;
2&:BEGIN;
3:SET ROLE role_concurrency_test;
3&:BEGIN;
SELECT * FROM rg_concurrency_view;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
1:END;
2<:
3<:
SELECT * FROM rg_concurrency_view;
1q:
2q:
3q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test2: terminate a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1:BEGIN;
2:SET ROLE role_concurrency_test;
2&:BEGIN;
3:SET ROLE role_concurrency_test;
3&:BEGIN;
SELECT * FROM rg_concurrency_view;
SELECT pg_terminate_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
1:END;
2<:
3<:
SELECT * FROM rg_concurrency_view;
1q:
2q:
3q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test3: cancel a query that is running
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1&:SELECT pg_sleep(10000);
2:SET ROLE role_concurrency_test;
2&:SELECT pg_sleep(10000);
6:SET ROLE role_concurrency_test;
6&:BEGIN;
7:SET ROLE role_concurrency_test;
7&:BEGIN;
SELECT * FROM rg_concurrency_view;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting='f' AND rsgname='rg_concurrency_test';
1<:
2<:
6<:
7<:
SELECT * FROM rg_concurrency_view;
1q:
2q:
6q:
7q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test4: terminate a query that is running
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1&:SELECT pg_sleep(10000);
2:SET ROLE role_concurrency_test;
2&:SELECT pg_sleep(10000);
6:SET ROLE role_concurrency_test;
6&:BEGIN;
7:SET ROLE role_concurrency_test;
7&:BEGIN;
SELECT * FROM rg_concurrency_view;
SELECT pg_terminate_backend(procpid) FROM pg_stat_activity WHERE waiting='f' AND rsgname='rg_concurrency_test';
1<:
2<:
6<:
7<:
SELECT * FROM rg_concurrency_view;
1q:
2q:
6q:
7q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

DROP VIEW rg_concurrency_view;
