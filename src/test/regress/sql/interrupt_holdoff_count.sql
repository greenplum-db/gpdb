-- test for Github Issue 15278
-- QD should reset InterruptHoldoffCount
-- start_ignore
create extension if not exists gp_inject_fault;
-- end_ignore

select gp_inject_fault('start_prepare', 'error', dbid, current_setting('gp_session_id')::int)
	from gp_segment_configuration where content = 0 and role = 'p';

create table t_15278(a int, b int);

-- Without fix, the above transaction will lead
-- QD's global var InterruptHoldoffCount not reset to 0
-- thus the below SQL will return t. After fixing, now
-- the below SQL will print an error message, this is
-- the correct behavior.
select pg_cancel_backend(pg_backend_pid());

select gp_inject_fault('start_prepare', 'reset', dbid, current_setting('gp_session_id')::int)
	from gp_segment_configuration where content = 0 and role = 'p';
