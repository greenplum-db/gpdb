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

