-- Test intends to validate tuple visibility between QE reader and writer in
-- presence of subtransactions

drop table if exists subxact1;
drop table if exists subxact2;
create table subxact1(a int, b int) distributed by (a);
create table subxact2(a int, b int) distributed by (a);
insert into subxact2 select i,i from generate_series(1, 12)i;
analyze subxact2;

--
-- Plpgsql functions
--

-- A recursive function to insert a value in subxact1 table and abort
-- after insert if specified.
create or replace function recurse(nest integer, value integer, abortme boolean)
returns void as
$$
declare
   i integer;
begin
   i = nest - 1;
   if i > 0 then
      perform recurse(i, value, abortme);
   end if;
   execute 'insert into subxact1 values ('|| value || ',' || value || ')';
   if abortme = true then
      raise exception 'abort me';
   end if;
exception
   when raise_exception then
      raise notice 'abort me %', value;
   when others then
      raise notice 'we should never get here';
end;
$$
language plpgsql;

-- TEST: all subtransactions committed without overflow
begin;
select recurse(10, 1, false);
select count(*) = 10 as passed from subxact1 where a = 1;
savepoint subtx_no_overflow;
-- Create a reader gang, and all tuples inserted by previous subtransactions
-- should be visible.  The MPP plan must be such that the subxact1 table is
-- scanned by the reader gang.  Analyzing subxact1 here causes such a plan to
-- be generated.  If analyze is omitted, the plan is such that reader gangs
-- scan subxact2, which will not result in the intended test scenario.
analyze subxact1;
select count(*) from subxact2 t2, subxact1 t1 where t2.a = t1.b;
commit;

-- TEST: all subtransactions committed with overflow
-- Recurse to a depth greater than PGPROC_MAX_CACHED_SUBXIDS
begin;
select recurse(80, 2, false);
select count(*) = 80 as passed from subxact1 where a = 2;
-- create a reader gang, and all tuples inserted by previous subtransactions
-- should be visible.
select count(*) from subxact2 t2, subxact1 t1 where t2.a = t1.b;
commit;

-- TEST: some subtransactions aborted with reader gang for cursor
-- Some subtransactions abort while more than PGPROC_MAX_CACHED_SUBXIDS of them
-- commit.  Tuples inserted by aborted subtransactions should not be
-- visible.
begin;
-- Keeping the value to be inserted (second argument) same is needed so that all
-- inserts are dispatched to the same segment.
select recurse(40, 3, false); -- committed
select recurse(10, 3, true); -- aborted
select recurse(40, 3, false); -- committed
declare c1 cursor for select count(*) = 80 as passed from subxact1 where a = 3;
-- Validate that rows inserted by committed subtransactions are
-- visible and rows inserted by aborted subtransactions aren't.
fetch all in c1;
select count(*) = 80 as passed from subxact2 t2, subxact1 t1
 where t2.a = t1.b and t1.a = 3;
commit;
