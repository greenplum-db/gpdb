\t
create temp table temp_table(i int);
create schema good_schema;
create table good_schema.good_table(i int);
\o test/behave/mgmt_utils/steps/data/gpcrondump/pid_leak
select pg_backend_pid();
begin;
select * from temp_table;
-- we are just picking an arbitrary number that's long enough for gpcrondump to finish
-- we will kill this process once the dump is done.
select pg_sleep(500)
