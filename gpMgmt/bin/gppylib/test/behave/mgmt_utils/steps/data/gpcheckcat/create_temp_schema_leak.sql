\t
create temp table temp_table(i int);
create schema good_schema;
create table good_schema.good_table(i int);
\o gppylib/test/behave/mgmt_utils/steps/data/gpcheckcat/pid_leak
select pg_backend_pid();
-- sleep for 5 seconds so as to be able to kill the process while the session is still running
select pg_sleep(5)
