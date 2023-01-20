-- reset the GUC and restart cluster.
-- start_ignore
! gpconfig -r gp_resource_manager;
! gpstop -rai;
SHOW gp_resource_manager;
-- end_ignore

-- reset settings
ALTER RESOURCE GROUP admin_group SET concurrency 10;
ALTER RESOURCE GROUP default_group SET concurrency 20;