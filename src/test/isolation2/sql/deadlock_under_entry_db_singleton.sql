CREATE TABLE deadlock_entry_db_singleton_table (c int, d int);
INSERT INTO deadlock_entry_db_singleton_table select i, i+1 from generate_series(1,10) i;

CREATE FUNCTION function_volatile(x int) RETURNS int AS $$ /*in func*/
BEGIN /*in func*/
	UPDATE deadlock_entry_db_singleton_table SET d = d + 1  WHERE c = $1; /*in func*/
	RETURN $1 + 1; /*in func*/
END $$ /*in func*/
LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

-- inject fault on QD
! gpfaultinjector -f transaction_start_under_entry_db_singleton -m async -y reset -o 0 -s 1;
! gpfaultinjector -f transaction_start_under_entry_db_singleton -m async -y suspend -o 0 -s 1;

-- now, the QD should already hold RowExclusiveLock and ExclusiveLock on `deadlock_entry_db_singleton_table`
-- the QE ENTRY_DB_SINGLETON will stop at the StartTransaction.
1&:UPDATE deadlock_entry_db_singleton_table set d = d + 1 FROM (select 1 from deadlock_entry_db_singleton_table, function_volatile(5)) t;

-- verify the fault hit
! gpfaultinjector -f transaction_start_under_entry_db_singleton -m async -y status -o 0 -s 1;

-- in separate session, try to update the `deadlock_entry_db_singleton_table` table, and it will wait for ExclusiveLock on `deadlock_entry_db_singleton_table`
2&: update deadlock_entry_db_singleton_table set d = d + 1;

-- we expect to hit deadlock
! gpfaultinjector -f transaction_start_under_entry_db_singleton -m async -y resume  -o 0 -s 1;
! gpfaultinjector -f transaction_start_under_entry_db_singleton -m async -y reset -o 0 -s 1;

-- verify the deadlock across multiple pids with same mpp session id
with lock_on_deadlock_entry_db_singleton_table as (select * from pg_locks where relation = 'deadlock_entry_db_singleton_table'::regclass and gp_segment_id = -1) 
select count(*) as FoundDeadlockProcess from lock_on_deadlock_entry_db_singleton_table 
where granted = false and mppsessionid in (
	select mppsessionid from lock_on_deadlock_entry_db_singleton_table where granted = true
);

-- join the session 1 and session 2
-- we expect to see an ERROR message here due to function_volatile function
-- tried to update table from ENTRY_DB_SINGLETON (which is read-only) in session 1
1<:
2<:
