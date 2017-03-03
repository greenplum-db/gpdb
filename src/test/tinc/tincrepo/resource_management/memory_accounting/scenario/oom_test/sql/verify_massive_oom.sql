-- @author ramans2
-- @created 2014-03-27 12:00:00
-- @modified 2014-03-27 12:00:00
-- @description Check segment log to verify memory usage and no ERROR/PANIC messages

-- SQL to check segment logs for memory usage
select logseverity, logstate, logmessage from gp_toolkit.__gp_log_segment_ext where logmessage like '%Logging memory usage%' and  logtime >= (select logtime from gp_toolkit.__gp_log_master_ext where logmessage like 'statement: select 5 as oom_test;' order by logtime desc limit 1) order by logtime desc limit 1;

-- SQL to check segment logs for ERROR or PANIC messages
select logseverity, logstate, logmessage from gp_toolkit.__gp_log_segment_ext where logstate = 'XX000' and  logtime >= (select logtime from gp_toolkit.__gp_log_master_ext where logmessage like 'statement: select 5 as oom_test;' order by logtime desc limit 1) order by logtime desc;
