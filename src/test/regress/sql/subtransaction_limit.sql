-- These tests generate more than MaxGpSavePoints subtransactions.
-- The objective is to test the spilling of in-progress
-- subtransactions array to disk (Ref:
-- AddSubtransactionsToSharedSnapshot).

drop table if exists subxact1;
create table subxact1(a int) distributed by (a);

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
   execute 'insert into subxact1 values ('|| value ||')';
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

-- Recurse to a depth greater than MaxGpSavePoints.  Recursion
-- increases depth (nesting level) of the subtransaction tree on every
-- call.
select recurse(105, -1, false);
select count(*) = 105 as passed from subxact1 where a = -1;


-- Some subtransactions abort while more than MaxGpSavePoints of them
-- commit.  Tuples inserted by aborted subtransactions should not be
-- visible.
begin;
create or replace function insert_loop(cnt integer) returns void as
$$
declare
   i integer;
begin
   i = cnt;
   while i > 0 loop
      if i % 17 = 0 then
         perform recurse(1, -2, true);
      else
         perform recurse(1, -3, false);
      end if;
      i = i - 1;
   end loop;
end;
$$
language plpgsql;
select insert_loop(110);
commit;
-- Validate that rows inserted by committed subtransactions are
-- visible and rows inserted by aborted subtransactions aren't.
select count(*) > 100 as passed from subxact1 where a = -3;
select count(*) = 0 as passed from subxact1 where a = -2;

--
-- Plpython functions to cross the MaxGpSavePoints limit.  Reuse
-- subxact1 table but insert a disjoint range of integers.
--

-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function pyrecurse(
	nest integer, value integer, abortme boolean)
	returns void as
$$
	i = nest - 1
	val = value
	abrt = abortme
	if i > 0:
		plpy.execute('select pyrecurse(%d, %d, %s)' % (i, val, abrt))
	if abrt == True:
		try:
			plpy.execute("insert into subxact1 values (%d), ('abc');" % val)
		except plpy.SPIError as e:
			# The plpy.execute() above starts a subtransaction and it
            # aborts because of 'abc'.  Catch the plpy exception here
            # so as to not let the top transaction abort.
			plpy.notice(str(e))
	else:
		plpy.execute('insert into subxact1 values (%d)' % val)
$$
language plpythonu;

-- Recurse to a depth greater than MaxGpSavePoints.
select pyrecurse(105, 1, false);
select count(*) = 105 as passed from subxact1 where a = 1;
-- Some subtransactions abort while more than MaxGpSavePoints of them
-- commit.
begin;
create or replace function pyinsert_loop(cnt integer) returns void as
$$
	for i in range(cnt):
		if i%17 == 0:
			plpy.execute('select pyrecurse(%d, %d, %s)' % (1, 2, True))
		else:
			plpy.execute('select pyrecurse(%d, %d, %s)' % (1, 3, False))
$$
language plpythonu;
select pyinsert_loop(110);
commit;
-- Validate that rows inserted by committed subtransactions are
-- visible while some subtransactions in the same tree did not commit.
select count(*) > 100 as passed from subxact1 where a = 3;
select count(*) = 0 as passed from subxact1 where a = 2;
