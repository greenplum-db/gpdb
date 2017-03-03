--start_ignore
drop table if exists cf_executor_test;
create table cf_executor_test (a integer);
insert into cf_executor_test select a from generate_series(1,100) a;
set log_min_messages=debug5;
--end_ignore

set debug_print_slice_table=on;
select count(*) from cf_executor_test;

--FOR UPDATE/FOR SHARE
select * from cf_executor_test order by a limit 1 for update;
select * from cf_executor_test order by a limit 1 for share;

--returning clause
insert into cf_executor_test values (1) returning *;

drop table cf_executor_test;
create table cf_executor_test (a integer);
insert into cf_executor_test select a from generate_series(1,5) a;

select * from cf_executor_test limit null;
select * from cf_executor_test limit 0;

--start_ignore
reset log_min_messages;
reset debug_print_slice_table;
drop table cf_executor_test;
--end_ignore
