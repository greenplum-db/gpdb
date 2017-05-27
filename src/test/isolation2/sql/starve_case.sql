-- This test validate NO starvation of waiting sessions
-- due to READER ignore waitMask

create table starve (c int);
create table starve_helper (c int);

CREATE OR REPLACE FUNCTION function_starve_volatile(x int) RETURNS int AS $$ /*in func*/
declare /*in func*/
  v int; /*in func*/
BEGIN /*in func*/
	SELECT count(c) into v FROM starve; /*in func*/
  RETURN $1 + 1; /*in func*/
END $$ /*in func*/
LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

-- hold access shared lock
1: begin;
1: select * from starve; 

-- waiting on access exclusive lock
2: begin;
2>: alter table starve rename column c to d;

select pg_sleep(1);

-- check the locks, expect session 2 wait
-- expect: 1 row with AccessExclusiveLock
select mode from pg_locks where granted=false and locktype = 'relation' and gp_segment_id = -1;

-- ENTRY_DB_SINGLETON reader request access share lock
3: begin;
3>: select * from starve_helper, function_starve_volatile(5);

select pg_sleep(1);

-- check the locks again, expect both session 2 and session 3 wait
-- expect: 2 rows with AccessExclusiveLock and AccessSharedLock
select mode from pg_locks where granted=false and locktype = 'relation' and gp_segment_id = -1;

-- session 1 release lock
1: commit;

-- session 2 join
2<:
2: commit;

-- session 3 join
-- CORRECT BEHAVIOR: session 3 should wait for session 2 because of waitMask
-- conflict, then we should see error column 'c' doesn't exist
3<:
3: commit;
