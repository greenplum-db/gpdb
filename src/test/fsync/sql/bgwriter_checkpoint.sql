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

begin;
create function num_dirty(relid oid) returns bigint as
$$
   select count(*) from dirty_buffers()
     as (tablespace oid, database oid, relfilenode oid, block int)
     where relfilenode in
           (select relfilenode from pg_class
            where oid=$1);
$$ language sql;

-- Wait until number of dirty buffers for the specified relfiles drops
-- to 0 or timeout occurs.  Returns false if timeout occured.
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
      select sum(num_dirty($1)) into d from gp_dist_random('gp_id');
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

-- Reset all faults.
\!gpfaultinjector -f all -m async -y reset -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

-- Start with a clean slate (no dirty buffers).
checkpoint;

-- Skip checkpoints.
\!gpfaultinjector -f checkpoint -m async -y skip -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

-- Suspend bgwriter.
\!gpfaultinjector -f fault_in_background_writer_main -m async -y suspend -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

-- Ensure no buffers are dirty before we start.
select * from dirty_buffers()
 as (tablespace oid, database oid, relfilenode oid, block int);
select dirty_buffers() from gp_dist_random('gp_id')
 as (tablespace oid, database oid, relfilenode oid, block int);

-- Make buffers dirty.  At least two relfiles must be sync'ed during
-- next checkpoint.
insert into fsync_test1 select i, i from generate_series(1,1000)i;
delete from fsync_test2;

-- Should return at least one dirty buffer.
select sum(num_dirty('fsync_test1'::regclass)) > 0 as passed
 from gp_dist_random('gp_id');
select sum(num_dirty('fsync_test2'::regclass)) > 0 as passed
 from gp_dist_random('gp_id');

-- Flush all dirty pages by BgBufferSync()
\!gpfaultinjector -f bg_buffer_sync_default_logic -m async -y skip -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

-- Resume bgwriter.
\!gpfaultinjector -f fault_in_background_writer_main -m async -y resume -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

-- Wait until bgwriter sweeps through and writes out dirty buffers.
-- The 10 indicates timeout in terms of number of iterations to be
-- executed by the waiting function.  Each iteration sleeps for .5
-- seconds.
select wait_for_bgwriter('fsync_test1'::regclass, 25) as passed;
select wait_for_bgwriter('fsync_test2'::regclass, 25) as passed;

-- Inject fault to count relfiles fsync'ed by checkpointer.
\!gpfaultinjector -f fsync_counter -m async -y skip -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

-- Resume checkpoints.
\!gpfaultinjector -f checkpoint -m async -y reset -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'

checkpoint;

-- There should be no dirty buffers after checkpoint.
select dirty_buffers() from gp_dist_random('gp_id')
 as (tablespace oid, database oid, relfilenode oid, block int);

-- Validate that the number of files fsync'ed by checkpointer is at
-- least 2.  The two files fsync'ed should be corresponding to
-- fsync_test1 and fsync_test2 tables. `num times hit` is corresponding
-- to the number of files synced by `fsync_counter` fault type.
\!if [[ $(gpfaultinjector -f fsync_counter -m async -y status -o 0 --seg_dbid 2 | grep 'num times hit' | sed -e 's/^.*num times hit[^0-9]*\([0-9]*\)[^0-9]*$/\1/') -gt 2 ]]; then echo "PASS: buffers synced"; else echo "FAIL: buffers not synced"; fi

-- Reset all faults.
\!gpfaultinjector -f all -m async -y reset -o 0 -H ALL -r primary | grep -i 'error\|fail\|done' | sed -e 's/.*DONE$/DONE/'
