-- Test the log related GUCs
set log_min_error_statement = error;

-- Case 1, test the log_min_error_statement GUC for coordinator log
-- the error statement will be logged as default
creat table log_aaa (id int, c text); -- this should raise error
select logseverity, logmessage, logdebug from gp_toolkit.__gp_log_coordinator_ext
where logseverity='ERROR' and logmessage like '%"creat"%' order by logtime desc limit 1;

-- should contain the log from elog_exception_statement()
select logseverity, logmessage, logdebug from gp_toolkit.__gp_log_coordinator_ext
where logseverity='LOG' and logmessage like '%exception was encountered%'
and logmessage not like '%__gp_log_coordinator_ext%'
order by logtime desc limit 1;

-- set log_min_error_statement to panic to skip log the error statement
set log_min_error_statement = panic;
creat table log_aaa (id int, c text); -- this should raise error
-- logdebug should be null
select logseverity, logmessage, logdebug from gp_toolkit.__gp_log_coordinator_ext
where logseverity='ERROR' and logmessage like '%"creat"%' order by logtime desc limit 1;

-- this should only show the two select and log from elog_exception_statement() is not included
select logseverity, logmessage, logdebug from gp_toolkit.__gp_log_coordinator_ext
where logseverity='LOG' and logmessage like '%exception was encountered%'
order by logtime desc limit 2;
set log_min_error_statement = error;


-- Case 2, test the log_min_error_statement GUC for segments log
-- the error statement will be logged as default
create table log_test(id int, c text);
insert into log_test select i, 'test' from generate_series(1, 10) as i;

-- use fault inject to trigger error
SELECT gp_inject_fault('qe_got_snapshot_and_interconnect', 'error', 2);
select * from log_test;
select gp_inject_fault('qe_got_snapshot_and_interconnect', 'reset', 2);
select logseverity, logmessage, logdebug from gp_toolkit.__gp_log_segment_ext
where logseverity='ERROR' and logsegment = 'seg0' and logmessage like '%fault triggered%'
order by logtime desc limit 1;

set log_min_error_statement = panic;
SELECT gp_inject_fault('qe_got_snapshot_and_interconnect', 'error', 2);
select * from log_test;
select gp_inject_fault('qe_got_snapshot_and_interconnect', 'reset', 2);
-- logdebug should be null
select logseverity, logmessage, logdebug from gp_toolkit.__gp_log_segment_ext
where logseverity='ERROR' and logsegment = 'seg0' and logmessage like '%fault triggered%'
order by logtime desc limit 1;

set log_min_error_statement = error;

