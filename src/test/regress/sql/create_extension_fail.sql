--
-- Tests for return correct error from qe when create extension error
-- The issue: https://github.com/greenplum-db/gpdb/issues/11304
--

drop extension if exists gp_debug_numsegments;
create extension if not exists gp_inject_fault;

select gp_inject_fault('create_function_fail', 'error', dbid) from gp_segment_configuration where role = 'p' and content = 2;

-- the following create exetension statement will fail due to fault-ibject
-- the failure happens during Create-UDFs for the extension on QEs, QDs
-- should catch the error and reset the global state variables in QEs correctly.
-- Previously, QD's error message is confusing, now it should print the direct
-- failing reason from QE instead of "query plan with multiple segworker groups is not supported".
create extension gp_debug_numsegments;

-- Next we need to test the global state vars of extension creating
-- is reset correctly in the above error handling. Since we do not
-- destroy gangs, the next create table statement will reuse the
-- write gang of the above create extension statement. If the
-- global vars (creating_extension, CurrentExtensionObject) are
-- not resetting, then the following create statement when executing
-- on QEs will consider itself need to bind the table to the
-- extension and add wrong dependency with a non-exist extension (the
-- above extension creation statement aborts). Finally we can never
-- drop the table. The cases verify this by showing we can still
-- drop the table from a new session. A new session to drop is critical
-- for this test since new session have a clean environment.
create table t_11304(a int);
-- start a new session so that make sure a new gang
\c
drop table t_11304;

select gp_inject_fault('create_function_fail', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 2;

--
-- Another Test from Issue: https://github.com/greenplum-db/gpdb/issues/12713
-- Similar comments please refer above case's
--
create role user_12713;
set role user_12713;
-- the will fail due to privilege
create extension gp_debug_numsegments;
create table t_12713(a int);
\c
drop table t_12713;
drop role user_12713;
