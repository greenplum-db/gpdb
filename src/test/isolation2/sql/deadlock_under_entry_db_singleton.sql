CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Test to validate that ENTRY_DB_SINGLETON reader does not cause a
-- deadlock.
--
-- The deadlock used to happen when ENTRY_DB_SINGLETON enters
-- LockAcquire with its writer already holding the lock.  And the
-- requested lockmode conflicts with another session's requested
-- lockmode who is waiting on the same lock.  The waitMask check in
-- LockAcquire would put the ENTRY_DB_SINGLETON into wait queue.  If
-- the ENTRY_DB_SINGLETON is supposed to produce some tuples that need
-- to be consumed by other nodes in the plan, either QEs or the writer
-- QD may wait for ENTRY_DB_SINGLETON and we will have a wait cycle,
-- causing deadlock.
--
-- This test creates one such scenario using a volatile funciton.  It
-- is critical that the function be volatile, otherwise,
-- ENTRY_DB_SIGNLETON reader is not spawned to execute the query.
--
-- This thread on gpdb-dev has more details:
-- https://groups.google.com/a/greenplum.org/d/msg/gpdb-dev/OS1-ODIK0P4/ZIzayBbMBwAJ

CREATE TABLE deadlock_entry_db_singleton_table (c int, d int);
INSERT INTO deadlock_entry_db_singleton_table select i, i+1 from generate_series(1,10) i;

-- Function that needs ExclusiveLock on a table.  Use a non-SQL
-- language for this function so that parser cannot understand its
-- definition.  That way, ExclusiveLock is requested during execution
-- of the function.  If the lock is acquired during plan generation of
-- the calling SQL statement, we don't get the deadlock.
CREATE FUNCTION function_volatile(x int) RETURNS int AS $$ /*in func*/
BEGIN /*in func*/
	UPDATE deadlock_entry_db_singleton_table SET d = d + 1  WHERE c = $1; /*in func*/
	RETURN $1 + 1; /*in func*/
END $$ /*in func*/
LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

-- inject fault on QD
select gp_inject_fault('transaction_start_under_entry_db_singleton', 'reset', 1);
select gp_inject_fault('transaction_start_under_entry_db_singleton', 'suspend', 1);

-- The QD should already hold RowExclusiveLock and ExclusiveLock on
-- deadlock_entry_db_singleton_table and the QE ENTRY_DB_SINGLETON
-- will stop at in StartTransaction due to suspend fault.
1&:UPDATE deadlock_entry_db_singleton_table set d = d + 1 FROM (select 1 from deadlock_entry_db_singleton_table, function_volatile(5)) t;

-- verify the fault hit
select gp_inject_fault('transaction_start_under_entry_db_singleton', 'status', 1);

-- This session will wait for ExclusiveLock on
-- deadlock_entry_db_singleton_table.
2&: update deadlock_entry_db_singleton_table set d = d + 1;

-- The ENTRY_DB_SINGLETON will acquire exclusive lock on
-- deadlock_entry_db_singleton_table.  The lockmode conflicts with the
-- lock's waitMask due to session2 already waiting for the same lock.
-- In spite of the waitMask conflict the lock should be granted to
-- ENTRY_DB_SINGLETON reader because its writer already holds the
-- lock.  Otherwise, we may have a deadlock.
select gp_inject_fault('transaction_start_under_entry_db_singleton', 'resume', 1);
select gp_inject_fault('transaction_start_under_entry_db_singleton', 'reset', 1);

-- verify the deadlock across multiple pids with same mpp session id
with lock_on_deadlock_entry_db_singleton_table as (select * from pg_locks
where relation = 'deadlock_entry_db_singleton_table'::regclass and gp_segment_id = -1)
select count(*) as FoundDeadlockProcess from lock_on_deadlock_entry_db_singleton_table
where granted = false and mppsessionid in (
	select mppsessionid from lock_on_deadlock_entry_db_singleton_table where granted = true
);

-- join the session1 and session2
-- we expect to see an ERROR message here due to function_volatile function
-- tried to update table from ENTRY_DB_SINGLETON (which is read-only) in session1
1<:
2<:
