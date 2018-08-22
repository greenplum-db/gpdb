--
-- Test to ensure that reader processes do not cause starvation of
-- processes already waiting on a lock.  Readers must wait on a lock
-- if their writer does not already hold the lock and the requested
-- lockmode conflicts with existing waiter's lockmode (waitMask
-- conflict).
--

create table starve (c int);
create table starve_helper (name varchar, sessionid int);

-- Function to access a table so that AccessShare lock is requested on
-- the table.  Use a non-SQL language for this function so that parser
-- cannot understand its definition.  That way, AccessShareLock is
-- requested during execution of the function.  If the lock is acquired
-- during plan generation of the calling SQL statement, the
-- ENTRY_DB_SINGLETON reader that executes this function won't go
-- through the waitMask conflict check in LockAcquire().
CREATE OR REPLACE FUNCTION function_starve_volatile(x int) /*in func*/
RETURNS int AS $$ /*in func*/
declare /*in func*/
  v int; /*in func*/
BEGIN /*in func*/
	SELECT count(c) into v FROM starve; /*in func*/
	RETURN $1 + 1; /*in func*/
END $$ /*in func*/
LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

-- Function to wait until a specific session is reported as waiting on
-- a lock.  The session's mppsessionid is obtained from starve_helper
-- table.  Timeout if no locks are awaited within 2 seconds.
CREATE OR REPLACE FUNCTION wait_until_locks_awaited(sess_name varchar) /*in func*/
RETURNS bool AS $$ /*in func*/
declare /*in func*/
	num_awaited int := 0; /*in func*/
	iterations int := 0; /*in func*/
	sessions_waiting_for_locks int[]; /*in func*/
begin /*in func*/
	while num_awaited = 0 and iterations < 20 loop /*in func*/
		select array_agg(mppsessionid) into sessions_waiting_for_locks
			from pg_locks where granted = false and gp_segment_id = -1; /*in func*/
		select count(*) into num_awaited from starve_helper s where /*in func*/
		s.name = sess_name and s.sessionid = ANY (sessions_waiting_for_locks); /*in func*/
		perform pg_sleep(.1); /*in func*/
		iterations := iterations + 1; /*in func*/
	end loop; /*in func*/
	return num_awaited > 0; /*in func*/
end $$ /*in func*/
LANGUAGE plpgsql STABLE;

-- Hold access shared lock, so that session2 must wait.
1: begin;
1: select * from starve;

2: insert into starve_helper select 'session2', setting::int from pg_settings
   where name = 'gp_session_id';
-- Wait on access exclusive lock.
2: begin;
2>: alter table starve rename column c to d;

select wait_until_locks_awaited('session2');

3: insert into starve_helper select 'session3', setting::int
   from pg_settings where name = 'gp_session_id';
-- ENTRY_DB_SINGLETON reader requests access share lock on table
-- starve.  The lockmode conflicts with already existing waiter's
-- lockmode (access exclusive).  And the writer is not holding any
-- lock on starve table.  So the reader must wait.
3: begin;
3>: select * from starve_helper, function_starve_volatile(5);

select wait_until_locks_awaited('session3');

-- Check the lock table, expect both session2 and session3 to wait.
-- expect: 2 rows with AccessExclusiveLock and AccessSharedLock
select mode from pg_locks where granted=false and relation='starve'::regclass
and gp_segment_id=-1;

-- Let everyone move forward.
1: commit;

-- session2 is granted the lock on starve table first.
2<:
2: select mode from pg_locks where granted=false and
   relation='starve'::regclass and gp_segment_id=-1;
2: commit;

-- session3 is granted the lock after session2 commits.  We should
-- see error column 'c' doesn't exist because session2 renamed it.
3<:
3: commit;


--
-- Test to ensure that writers do not starve processes already waiting
-- on a lock in case of waitMask conflict.
--
truncate table starve_helper;

-- Hold access shared lock, so that session2 must wait.
1: begin;
1: select * from starve;

2: insert into starve_helper select 'session2', setting::int from
   pg_settings where name = 'gp_session_id';
-- Wait on access exclusive lock.
2: begin;
2>: alter table starve add column e int default 0;

select wait_until_locks_awaited('session2');

3: insert into starve_helper select 'session3', setting::int from pg_settings
   where name = 'gp_session_id';
3: begin;
-- Wait on RowExclusiveLock on table starve because session2 is
-- waiting on the same lock with a conflicting lockmode.
3>: insert into starve values (1), (2);

select wait_until_locks_awaited('session3');

1: commit;
-- Session2 must go first.
2<:
-- Ensure that session3 is still waiting.
2: select mode from pg_locks where granted=false and relation='starve'::regclass
   and gp_segment_id=-1;
2: commit;

-- Session3 gets the lock only after session2 commits.
3<:
3: commit;
