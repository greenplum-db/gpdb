select *
  from gp_toolkit.gp_resgroup_config
 where groupname='default_group';

select rsgname
     , groupid
     , num_running
     , num_queueing
     , num_queued
     , num_executed
     , cpu_usage->'-1' as qd_cpu_usage
     , memory_usage->'-1'->'used' as qd_memory_used
     , memory_usage->'-1'->'shared_used' as qd_memory_shared_used
  from gp_toolkit.gp_resgroup_status
 where rsgname='default_group';

select rsgname
     , groupid
     , cpu
     , memory_used
     , memory_shared_used
  from gp_toolkit.gp_resgroup_status_per_host s
  join gp_segment_configuration c
    on s.hostname=c.hostname and c.content=-1 and role='p'
 where rsgname='default_group';

select rsgname
     , groupid
     , segment_id
     , cpu
     , memory_used
     , memory_shared_used
  from gp_toolkit.gp_resgroup_status_per_segment
 where rsgname='default_group'
   and segment_id=-1;

-- also log the raw output of the views, if any of above tests failed it is
-- easier to find out the causes with these logs.

-- start_ignore
select * from gp_toolkit.gp_resgroup_config;
select * from gp_toolkit.gp_resgroup_status;
select * from gp_toolkit.gp_resgroup_status_per_host;
select * from gp_toolkit.gp_resgroup_status_per_segment;
-- end_ignore
