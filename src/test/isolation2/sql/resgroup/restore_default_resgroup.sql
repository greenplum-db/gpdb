-- enable resource group and restart cluster.
-- start_ignore
! gpconfig -c gp_resource_group_cpu_limit -v 0.9;
! gpconfig -c gp_resource_group_memory_limit -v 0.9;
! gpconfig -c max_resource_groups -v 100;
! gpconfig -c max_connections -v 600 -m 150;
! gpstop -rai;
-- end_ignore

show gp_resource_manager;
show gp_resource_group_cpu_limit;
show gp_resource_group_memory_limit;
show max_connections;

-- by default admin_group has concurrency set to -1 which leads to
-- very small memory quota for each resgroup slot, correct it.
ALTER RESOURCE GROUP admin_group SET concurrency 150;
