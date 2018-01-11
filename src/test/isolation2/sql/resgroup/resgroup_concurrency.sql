-- test1: test gp_toolkit.gp_resgroup_status and pg_stat_activity
-- create a resource group when gp_resource_manager is queue
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- no query has been assigned to the this group

SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
2:SET ROLE role_concurrency_test;
2:BEGIN;
3:SET ROLE role_concurrency_test;
3:BEGIN;
4:SET ROLE role_concurrency_test;
4&:BEGIN;

-- new transaction will be blocked when the concurrency limit of the resource group is reached.
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT waiting_reason, rsgqueueduration > '0'::interval as time from pg_stat_activity where current_query = 'BEGIN;' and rsgname = 'rg_concurrency_test';
2:END;
3:END;
4<:
4:END;
2q:
3q:
4q:
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test2: test alter concurrency
-- Create a resource group with concurrency=2. Prepare 2 running transactions and 1 queueing transactions.
-- Alter concurrency 2->3, the queueing transaction will be woken up, the 'value' and 'proposed' of pg_resgroupcapability will be set to 3.
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
12:SET ROLE role_concurrency_test;
12:BEGIN;
13:SET ROLE role_concurrency_test;
13:BEGIN;
14:SET ROLE role_concurrency_test;
14&:BEGIN;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 3;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
12:END;
13:END;
14<:
14:END;
12q:
13q:
14q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test3: test alter concurrency
-- Create a resource group with concurrency=3. Prepare 3 running transactions, and 1 queueing transaction.
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=3, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
22:SET ROLE role_concurrency_test;
22:BEGIN;
23:SET ROLE role_concurrency_test;
23:BEGIN;
24:SET ROLE role_concurrency_test;
24:BEGIN;
25:SET ROLE role_concurrency_test;
25&:BEGIN;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- Alter concurrency 3->2, the 'proposed' of pg_resgroupcapability will be set to 2.
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- When one transaction is finished, queueing transaction won't be woken up. There're 2 running transactions and 1 queueing transaction.
24:END;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- New transaction will be queued, there're 2 running and 2 queueing transactions.
24&:BEGIN;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- Finish another transaction, one queueing transaction will be woken up, there're 2 running transactions and 1 queueing transaction.
22:END;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- Alter concurrency 2->2, the 'value' and 'proposed' of pg_resgroupcapability will be set to 2.
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- Finish another transaction, one queueing transaction will be woken up, there're 2 running transactions and 0 queueing transaction.
23:END;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
24<:
25<:
25:END;
24:END;
22q:
23q:
24q:
25q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test4: concurrently drop resource group

DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- DROP should fail if there're running transactions
32:SET ROLE role_concurrency_test;
32:BEGIN;
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
32:END;

DROP ROLE IF EXISTS role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test5: QD exit before QE 
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
51:SET ROLE role_concurrency_test;
51:BEGIN;
52:SET ROLE role_concurrency_test;
52&:BEGIN;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
52<:
52&:BEGIN;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
52<:
51q:
52q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test6: cancel a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
51:SET ROLE role_concurrency_test;
51:BEGIN;
52:SET ROLE role_concurrency_test;
52&:BEGIN;
51q:
52<:
52q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;


-- test7: terminate a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61:SET ROLE role_concurrency_test;
61:BEGIN;
62:SET ROLE role_concurrency_test;
62&:BEGIN;
SELECT pg_terminate_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
62<:
61q:
62q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test8: create a resgroup with concurrency=0
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=0, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61:SET ROLE role_concurrency_test;
61&:BEGIN;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
61<:
61q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test9: SET command should be bypassed
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=0, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61: SET ROLE role_concurrency_test;
61&: SELECT 1;
ALTER RESOURCE GROUP rg_concurrency_test set concurrency 1;
61<:
ALTER RESOURCE GROUP rg_concurrency_test set concurrency 0;
61: SET enable_hashagg to on;
61: SHOW enable_hashagg;
61: invalid_syntax;
61q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

--
-- Test cursors, pl/* functions only take one slot.
--
-- set concurrency to 1
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- declare cursors and verify that it only takes one resource group slot
71:SET ROLE TO role_concurrency_test;
71:CREATE TABLE foo_concurrency_test as select i as c1 , i as c2 from generate_series(1, 1000) i;
71:CREATE TABLE bar_concurrency_test as select i as c1 , i as c2 from generate_series(1, 1000) i;
71:BEGIN;
71:DECLARE c1 CURSOR for select c1, c2 from foo_concurrency_test order by c1 limit 10;
71:DECLARE c2 CURSOR for select c1, c2 from bar_concurrency_test order by c1 limit 10;
71:DECLARE c3 CURSOR for select count(*) from foo_concurrency_test t1, bar_concurrency_test t2 where t1.c2 = t2.c2;
71:
71:Fetch ALL FROM c1;
71:Fetch ALL FROM c2;
71:Fetch ALL FROM c3;
71:END;

-- create a pl function and verify that it only takes one resource group slot.
CREATE OR REPLACE FUNCTION func_concurrency_test () RETURNS integer as /*in func*/
$$ /*in func*/
DECLARE /*in func*/
	tmprecord RECORD; /*in func*/
	ret integer; /*in func*/
BEGIN /*in func*/
	SELECT count(*) INTO ret FROM foo_concurrency_test;	 /*in func*/
	FOR tmprecord IN SELECT * FROM bar_concurrency_test LOOP /*in func*/
		SELECT count(*) INTO ret FROM foo_concurrency_test;	 /*in func*/
	END LOOP; /*in func*/
 /*in func*/
	select 1/0; /*in func*/
EXCEPTION /*in func*/
	WHEN division_by_zero THEN /*in func*/
		SELECT count(*) INTO ret FROM foo_concurrency_test;	 /*in func*/
		raise NOTICE 'divided by zero'; /*in func*/
	RETURN ret; /*in func*/
END; /*in func*/
$$ /*in func*/
LANGUAGE plpgsql;

71: select func_concurrency_test();

-- Prepare/execute statements and verify that it only takes one resource group slot. 
71:BEGIN;
71:PREPARE p1 (integer) as select * from foo_concurrency_test where c2=$1;
71:PREPARE p2 (integer) as select * from bar_concurrency_test where c2=$1;
71:EXECUTE p1(1);
71:EXECUTE p2(2);
71:END;
71:PREPARE p3 (integer) as select * from foo_concurrency_test where c2=$1;
71:PREPARE p4 (integer) as select * from bar_concurrency_test where c2=$1;
71:EXECUTE p3(1);
71:EXECUTE p4(2);

DROP TABLE foo_concurrency_test;
DROP TABLE bar_concurrency_test;
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
