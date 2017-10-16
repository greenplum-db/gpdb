-- create a resource group when gp_resource_manager is queue
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH
(concurrency=1, cpu_rate_limit=20, memory_limit=60, memory_shared_quota=0, memory_spill_ratio=10);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

CREATE OR REPLACE VIEW rg_activity_status AS
	SELECT rsgname, waiting_reason, current_query
	FROM pg_stat_activity
	WHERE rsgname='rg_concurrency_test';

--
-- 1. increase concurrency after pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;

11:SET ROLE role_concurrency_test;
11:BEGIN;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21&:BEGIN;
22&:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;

SELECT * FROM rg_activity_status;

11:END;
11q:
21<:
22<:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

--
-- 2. increase concurrency before pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;

11:SET ROLE role_concurrency_test;
11:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21&:BEGIN;
22&:BEGIN;

SELECT * FROM rg_activity_status;

11:END;
11q:
21<:
22<:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

--
-- 3. increase both concurrency & memory_shared_quota after pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 60;

11:SET ROLE role_concurrency_test;
11:BEGIN;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21&:BEGIN;
22&:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;

SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 20;

21<:

SELECT * FROM rg_activity_status;

11:END;
11q:
22<:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

--
-- 4. increase both concurrency & memory_shared_quota before pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 60;

11:SET ROLE role_concurrency_test;
11:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 20;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21:BEGIN;
22&:BEGIN;

SELECT * FROM rg_activity_status;

11:END;
11q:
22<:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

--
-- 5. increase both concurrency & memory_limit after pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 0;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_LIMIT 30;

-- proc 11 gets a quota of 30/1=30
11:SET ROLE role_concurrency_test;
11:BEGIN;

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21&:BEGIN;
22&:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
-- now a new query needs a quota of 30/2=15 to run,
-- there is no free quota at the moment, so 21 & 22 are still pending
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_LIMIT 50;
-- now a new query needs a quota of 50/2=25 to run,
-- but there is only 50-30=20 free quota, so 21 & 22 are still pending
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_LIMIT 60;
-- now a new query needs a quota of 60/2=30 to run,
-- and there is 60-30=30 free quota, so 21 gets executed and 22 is still pending

21<:

SELECT * FROM rg_activity_status;

11:END;
-- 11 releases its quota, so there is now 30 free quota,
-- so 22 gets executed
11q:
22<:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

--
-- 6. increase both concurrency & memory_limit before pending queries
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 0;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_LIMIT 30;

-- proc 11 gets a quota of 30/1=30
11:SET ROLE role_concurrency_test;
11:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
-- now a new query needs a quota of 30/2=15 to run,
-- there is no free quota at the moment
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_LIMIT 60;
-- now a new query needs a quota of 60/2=30 to run,
-- and there is 60-30=30 free quota,
-- so one new query can get executed immediately

21:SET ROLE role_concurrency_test;
22:SET ROLE role_concurrency_test;
21:BEGIN;
-- proc 21 gets executed, there is no free quota now,
-- so proc 22 is pending
22&:BEGIN;

SELECT * FROM rg_activity_status;

11:END;
-- 11 releases its quota, so there is now 30 free quota,
-- so 22 gets executed
11q:
22<:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT pg_sleep(0.5);

SELECT * FROM rg_activity_status;

--
-- 7. decrease concurrency
--
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_LIMIT 50;
ALTER RESOURCE GROUP rg_concurrency_test SET MEMORY_SHARED_QUOTA 0;
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 10;
11:SET ROLE role_concurrency_test;
11:BEGIN;

12:SET ROLE role_concurrency_test;
12:BEGIN;

13:SET ROLE role_concurrency_test;
13:BEGIN;

14:SET ROLE role_concurrency_test;
14:BEGIN;

15:SET ROLE role_concurrency_test;
15:BEGIN;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;

11q:
12q:
13q:
14q:
15q:

--
-- 8. increase concurrency from 0
--
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
CREATE RESOURCE GROUP rg_concurrency_test WITH
(concurrency=0, cpu_rate_limit=20, memory_limit=60, memory_shared_quota=0, memory_spill_ratio=10);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

11:SET ROLE role_concurrency_test;
11&:BEGIN;
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;

11<:
SELECT * FROM rg_activity_status;

11:END;
11q:

--
-- 9.1 decrease concurrency to 0,
-- without running queries,
-- without pending queries.
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 0;
SELECT * FROM rg_activity_status;

--
-- 9.2 decrease concurrency to 0,
-- with running queries,
-- without pending queries.
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
SELECT * FROM rg_activity_status;

11:SET ROLE role_concurrency_test;
11:BEGIN;
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 0;
SELECT * FROM rg_activity_status;

11:END;
11q:

--
-- 9.3 decrease concurrency to 0,
-- with running queries,
-- with pending queries.
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
SELECT * FROM rg_activity_status;

11:SET ROLE role_concurrency_test;
11:BEGIN;
12:SET ROLE role_concurrency_test;
12&:BEGIN;
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 0;
SELECT * FROM rg_activity_status;

11:END;
11q:
SELECT * FROM rg_activity_status;

SELECT pg_cancel_backend(procpid) FROM pg_stat_activity
WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
12<:
12q:
SELECT * FROM rg_activity_status;

-- 10: drop a resgroup with concurrency=0 and pending queries
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=0, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61:SET ROLE role_concurrency_test;
61&:BEGIN;

ALTER ROLE role_concurrency_test RESOURCE GROUP none;
DROP RESOURCE GROUP rg_concurrency_test;

61<:
61:END;
61q:

-- 11: drop a role with concurrency=0 and pending queries
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=0, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61:SET ROLE role_concurrency_test;
61&:BEGIN;

DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

61<:
61q:

-- cleanup
-- start_ignore
DROP VIEW rg_activity_status;
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
