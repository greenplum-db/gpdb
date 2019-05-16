1:create table terminate_backend_t (a int) distributed by (a);

-- fault on seg1 to block insert command into terminate_backend_t table
select gp_inject_fault('heap_insert', 'infinite_loop', '', '',
   'terminate_backend_t', 1, 1, 0, dbid) from gp_segment_configuration
   where content = 1 and role = 'p';

-- expect this command to be terminated by 'test pg_terminate_backend'
1&: insert into terminate_backend_t values (1);

select gp_wait_until_triggered_fault('heap_insert', 1, dbid)
from gp_segment_configuration where content = 1 and role = 'p';

-- extract the pid for the previous query
SELECT pg_terminate_backend(pid,'test pg_terminate_backend')
FROM pg_stat_activity WHERE query like 'insert into terminate_backend_t%'
ORDER BY pid LIMIT 1;

-- EXPECT: session 1 terminated with 'test pg_terminate_backend'
1<:

-- query backend to ensure no PANIC on postmaster
select gp_inject_fault('heap_insert', 'reset', dbid)
   from gp_segment_configuration
   where content = 1 and role = 'p';

-- the table should be empty if insert was terminated
select * from terminate_backend_t;
