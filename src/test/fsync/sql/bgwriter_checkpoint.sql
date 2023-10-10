-- This test validates that dirty relfiles are sync'ed by checkpoint
-- process.  It simulates the following scenario.
--
--   * Start with a clean slate - ensure that all dirty buffers are
--     flushed by creating a checkpoint.
--   * Suspend bgwriter at the beginning of main loop.
--   * Suspend checkpointer process by injecting skip fault.
--   * Make buffers of two tables dirty (fsync_test1 and fsync_test2).
--   * Resume bgwriter so that it performs a round of writing out dirty
--     buffers.
--   * Resume checkpointer and let it fsync the two dirty relfiles
--     corresponding to fsync_test1 and fsync_test2 tables.
--   * Verify that at least two files were fsync'ed by checkpointer.
--
-- the hit times of fsync_counter is undetermined, we accept anything
-- between 4-9.
-- start_matchsubs
-- m/num times hit:\'[4-9]\'/
-- s/num times hit:\'[4-9]\'/num times hit:\'greater_than_two\'/
-- end_matchsubs

-- Prevent autovacuum from dirty-ing buffers.

-- start_ignore
\! gpconfig -c autovacuum -v off;
\! gpstop -au;
-- end_ignore

begin;
create function num_dirty_on_qes(relid oid) returns setof bigint as
$$
declare
  rfnode oid;
  result int4;
begin
   select relfilenode into rfnode from pg_class where oid=$1;

   select count(*) into result from dirty_buffers_on_qes()
     as (tablespace oid, database oid, relfilenode oid, block int)
     where relfilenode = rfnode;
   return next result;
end
$$ language plpgsql execute on all segments;

-- Wait until number of dirty buffers for the specified relfiles drops
-- to 0 or timeout occurs.  Returns false if timeout occurred.
create function wait_for_bgwriter(
   relid oid,
   timeout int)
returns boolean as
$$
declare
   d bigint;
   i  int;
begin
   i := 0;
   loop
      select sum(nd) into d from num_dirty_on_qes($1) nd;
      if (d = 0) then
         return true;
      end if;
      if i >= $2 then
         return false;
      end if;
      perform pg_sleep(.5);
      i := i + 1;
   end loop;
end;
$$ language plpgsql;

create table fsync_test1(a int, b int) distributed by (a);
create table fsync_test2(a int, b int) distributed by (a);
insert into fsync_test1 select i, i from generate_series(1,100)i;
insert into fsync_test2 select -i, i from generate_series(1,100)i;
end;

vacuum pg_proc;
-- Reset all faults.
-- 
-- NOTICE: important.
--
-- we use gp_inject_fault_infinite here instead of
-- gp_inject_fault so cache of pg_proc that contains
-- gp_inject_fault_infinite is loaded before checkpoint and
-- the following gp_inject_fault_infinite don't dirty the
-- buffer again.
select gp_inject_fault_infinite('all', 'reset', dbid) from gp_segment_configuration;

-- Start with a clean slate (no dirty buffers).
checkpoint;

-- Skip checkpoints.
select gp_inject_fault_infinite('checkpoint', 'skip', dbid)
from gp_segment_configuration where role = 'p' and content > -1;

-- Suspend bgwriter.
select gp_inject_fault_infinite('fault_in_background_writer_main', 'suspend', dbid)
from gp_segment_configuration where role = 'p' and content > -1;

-- Ensure no buffers are dirty before we start.
select * from dirty_buffers_on_qd()
 as (tablespace oid, database oid, relfilenode oid, block int);
select * from dirty_buffers_on_qes()
 as (tablespace oid, database oid, relfilenode oid, block int);

-- Make buffers dirty.  At least two relfiles must be sync'ed during
-- next checkpoint.
insert into fsync_test1 select i, i from generate_series(1,1000)i;
delete from fsync_test2;

-- Should return at least one dirty buffer.
select sum(nd) > 0 as passed from num_dirty_on_qes('fsync_test1'::regclass) nd;
select sum(nd) > 0 as passed from num_dirty_on_qes('fsync_test2'::regclass) nd;

-- Flush all dirty pages by BgBufferSync()
select gp_inject_fault_infinite('bg_buffer_sync_default_logic', 'skip', dbid)
from gp_segment_configuration where role = 'p' and content > -1;

-- Resume bgwriter.
select gp_inject_fault('fault_in_background_writer_main', 'resume', dbid)
from gp_segment_configuration where role = 'p' and content > -1;

-- Wait until bgwriter sweeps through and writes out dirty buffers.
-- The 10 indicates timeout in terms of number of iterations to be
-- executed by the waiting function.  Each iteration sleeps for .5
-- seconds.
select wait_for_bgwriter('fsync_test1'::regclass, 25) as passed;
select wait_for_bgwriter('fsync_test2'::regclass, 25) as passed;

-- Inject fault to count relfiles fsync'ed by checkpointer.
select gp_inject_fault_infinite('fsync_counter', 'skip', dbid)
from gp_segment_configuration where role = 'p' and content > -1;

-- Resume checkpoints.
select gp_inject_fault('checkpoint', 'reset', dbid)
from gp_segment_configuration where role = 'p' and content > -1;

checkpoint;

-- There should be no dirty buffers after checkpoint.
select * from dirty_buffers_on_qes()
 as (tablespace oid, database oid, relfilenode oid, block int);

-- Validate that the number of files fsync'ed by checkpointer is at
-- least 2.  The two files fsync'ed should be corresponding to
-- fsync_test1 and fsync_test2 tables. `num times hit` is corresponding
-- to the number of files synced by `fsync_counter` fault type.
select gp_inject_fault('fsync_counter', 'status', 2::smallint);

-- Reset all faults.
select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration;

-- Reset autovacuum;
-- start_ignore
\! gpconfig -c autovacuum -v on;
\! gpstop -au;
-- end_ignore
