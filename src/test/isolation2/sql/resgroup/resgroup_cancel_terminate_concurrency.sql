-- test1: cancel a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE OR REPLACE VIEW rg_concurrency_view AS
  SELECT wait_event_type IS NOT NULL as waiting, wait_event_type, state, query, rsgname
  FROM pg_stat_activity
  WHERE rsgname='rg_concurrency_test';

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_hard_quota_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1:BEGIN;
2:SET ROLE role_concurrency_test;
2&:BEGIN;
3:SET ROLE role_concurrency_test;
3&:BEGIN;
SELECT * FROM rg_concurrency_view;
SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE wait_event_type='ResourceGroup' AND rsgname='rg_concurrency_test';
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

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_hard_quota_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1:BEGIN;
2:SET ROLE role_concurrency_test;
2&:BEGIN;
3:SET ROLE role_concurrency_test;
3&:BEGIN;
SELECT * FROM rg_concurrency_view;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE wait_event_type='ResourceGroup' AND rsgname='rg_concurrency_test';
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

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_hard_quota_limit=20);
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
SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE wait_event_type = 'Timeout' AND rsgname='rg_concurrency_test';
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

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_hard_quota_limit=20);
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
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE wait_event_type = 'Timeout' AND rsgname='rg_concurrency_test';
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

-- test5: terminate a query waiting for a slot, that opens a transaction on exit callback
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_hard_quota_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
1:SET ROLE role_concurrency_test;
1:CREATE TEMP TABLE tmp(a INT);
2:SET ROLE role_concurrency_test;
2:BEGIN;
1&:SELECT 1;
SELECT * FROM rg_concurrency_view;
-- Upon receiving the terminate request, session 1 should start a new transaction to cleanup temp table.
-- Note, that session 1 has already been waiting for resource group slot, its new transaction will bypass
-- resource group since it's exiting.
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE wait_event_type='ResourceGroup' AND rsgname='rg_concurrency_test';
1<:
2:COMMIT;
SELECT * FROM rg_concurrency_view;
1q:
2q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

DROP VIEW rg_concurrency_view;
