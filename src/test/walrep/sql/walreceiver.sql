-- negative cases
SELECT test_send();
SELECT test_disconnect();

-- Wait until number of replication sessions drop to 0 or timeout
-- occurs. Returns false if timeout occurred.
create function check_and_wait_for_replication(
   timeout int)
returns boolean as
$$
declare
   d bigint;
   i  int;
begin
   i := 0;
   loop
      SELECT count(*) into d FROM pg_stat_replication where application_name = 'walreceiver_test';
      if (d = 0) then
         return true;
      end if;
      if i >= $1 then
         return false;
      end if;
      perform pg_sleep(.5);
      i := i + 1;
   end loop;
end;
$$ language plpgsql;

-- Avoid generating spurious WAL records by hint bit updates. The test below
-- is quite sensitive.
vacuum freeze;

-- Test connection
SELECT test_connect('application_name=walreceiver_test');
-- Should report 1 replication
SELECT count(*) FROM pg_stat_replication where application_name = 'walreceiver_test';
SELECT test_disconnect();
SELECT check_and_wait_for_replication(10);

-- Test connection passing hostname
SELECT test_connect('host=localhost application_name=walreceiver_test');
SELECT count(*) FROM pg_stat_replication where application_name = 'walreceiver_test';
SELECT test_disconnect();
SELECT check_and_wait_for_replication(10);

-- remember current_xlog_location.
-- start_ignore
select pg_current_xlog_location() as lsn;
-- end_ignore
\gset

-- lets generate some xlogs
create table testwalreceiver(a int);
insert into testwalreceiver select * from generate_series(0, 9);

-- Connect and receive the xlogs, validate everything was received from start to
-- end
SELECT test_connect('');
SELECT test_receive_and_verify(:'lsn', pg_current_xlog_location());
SELECT test_send();
SELECT test_disconnect();
