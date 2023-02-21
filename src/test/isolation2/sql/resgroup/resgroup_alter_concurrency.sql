-- create a resource group when gp_resource_manager is queue
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH(concurrency=1, cpu_hard_quota_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- After a 'q' command the client connection is disconnected but the
-- QD may still be alive, if we then query pg_stat_activity quick enough
-- we might still see this session with query '<IDLE>'.
-- A filter is put to filter out this kind of quitted sessions.
CREATE OR REPLACE VIEW rg_activity_status AS
	SELECT rsgname, wait_event_type, state, query
	FROM pg_stat_activity
	WHERE rsgname='rg_concurrency_test'
	  AND query <> '<IDLE>';

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

11:END;
11q:
21<:
22<:

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

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
21:BEGIN;
22&:BEGIN;

SELECT * FROM rg_activity_status;

11:END;
11q:
22<:

SELECT * FROM rg_activity_status;

21:END;
22:END;
21q:
22q:

SELECT * FROM rg_activity_status;

--
-- 3. decrease concurrency
--
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
-- start_ignore
-- The 'q' command returns before the underlying segments all actually quit,
-- so a following DROP command might fail.  Add a delay here as a workaround.
SELECT pg_sleep(1);
-- end_ignore

--
-- 4. increase concurrency from 0
--
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
CREATE RESOURCE GROUP rg_concurrency_test WITH(concurrency=0, cpu_hard_quota_limit=20);
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
-- 5.1 decrease concurrency to 0,
-- without running queries,
-- without pending queries.
--

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 1;
SELECT * FROM rg_activity_status;

ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 0;
SELECT * FROM rg_activity_status;

--
-- 5.2 decrease concurrency to 0,
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
-- 5.3 decrease concurrency to 0,
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

SELECT pg_cancel_backend(pid) FROM pg_stat_activity
WHERE wait_event_type='ResourceGroup' AND rsgname='rg_concurrency_test';
12<:
12q:
SELECT * FROM rg_activity_status;

-- 6: drop a resgroup with concurrency=0 and pending queries
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=0, cpu_hard_quota_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61:SET ROLE role_concurrency_test;
61&:BEGIN;

ALTER ROLE role_concurrency_test RESOURCE GROUP none;
DROP RESOURCE GROUP rg_concurrency_test;

61<:
61:END;
61q:

-- 7: drop a role with concurrency=0 and pending queries
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=0, cpu_hard_quota_limit=20);
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

