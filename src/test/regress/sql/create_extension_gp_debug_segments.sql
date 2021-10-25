--
-- Tests for return correct error from qe when create extension error
-- The issue: https://github.com/greenplum-db/gpdb/issues/11304
--

--start_ignore
drop extension if exists gp_debug_numsegments;
create extension if not exists gp_inject_fault;
--end_ignore

select gp_inject_fault('create_function_fail', 'error', 2);
create extension gp_debug_numsegments;

select gp_inject_fault('create_function_fail', 'reset', 2);

--
-- Tests for cannot drop table after create extension with normal user
-- The issue: https://github.com/greenplum-db/gpdb/issues/12713
--
--1. use normal user to create extension
--2. create a table
--3. quit psql
--4. reconnect
--5. try to drop the table

--start_ignore
drop table if exists t_12713;
drop user if exists normal_user_12713;
create user normal_user_12713;
--end_ignore

set role normal_user_12713;
create extension file_fdw;
create table t_12713(name text);
\c
drop table t_12713;
drop user normal_user_12713;
