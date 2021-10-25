--
-- Tests for return correct error from qe when create extension error
-- The issue: https://github.com/greenplum-db/gpdb/issues/11304
--

--start_ignore
drop extension if exists gp_debug_numsegments;
create extension if not exists gp_inject_fault;
--end_ignore

select gp_inject_fault('create_function_fail', 'error', dbid) from gp_segment_configuration where role = 'p' and content = 1;
create extension gp_debug_numsegments;
create table t_11304(a int);
select gp_inject_fault('create_function_fail', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 1;
\c
drop table t_11304;

-- Test for Github Issue https://github.com/greenplum-db/gpdb/issues/12703
create table t_12703(a int);
insert into t_12703 values (1);

begin;
declare myc CURSOR for select * from t_12703;
select gp_inject_fault('destroy_gang_namedportal_risk_creategang', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = -1;
savepoint xxx;
abort;
select gp_inject_fault('destroy_gang_namedportal_risk_creategang', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

begin;
declare myc CURSOR for select * from t_12703;
select gp_inject_fault('destroy_gang_namedportal_risk_copy', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = -1;
copy t_12703 from stdin;
1
\.
abort;
select gp_inject_fault('destroy_gang_namedportal_risk_copy', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;
