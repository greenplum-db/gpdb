create resource group rg_wait_time_test with (concurrency=2, cpu_max_percent=20);
create role role_wait_time_test resource group rg_wait_time_test;

1: set role role_wait_time_test;
1: begin;

2: set role role_wait_time_test;
2: begin;

3: set role role_wait_time_test;
-- this group only has two concurrency, the below SQL will be put in wait queue
3&: begin;

select pg_sleep(2);

-- commit the 1st transaction to unblock 3
-- so 3 will record its wait time
1: end;
2: end;
3<:

select total_queue_duration > '00:00:00' waited
from gp_toolkit.gp_resgroup_status where groupname = 'rg_wait_time_test';

3:end;
1q:
2q:
3q:

drop role role_wait_time_test;
drop resource group rg_wait_time_test;

create resource group rg_wait_time_test with (concurrency=2, cpu_max_percent=20);
create role role_wait_time_test resource group rg_wait_time_test;

1: set role role_wait_time_test;
1: begin;

2: set role role_wait_time_test;
2: begin;

3: set role role_wait_time_test;
-- this group only has two concurrency, the below SQL will be put in wait queue
3&: begin;

select pg_sleep(2);

select pg_terminate_backend(pid)
from pg_stat_activity
where rsgname = 'rg_wait_time_test' and wait_event_type = 'ResourceGroup';

1q:
2q:
3<:
3q:

select total_queue_duration > '00:00:00' waited
from gp_toolkit.gp_resgroup_status where groupname = 'rg_wait_time_test';

-- clean up
drop role role_wait_time_test;
drop resource group rg_wait_time_test;
