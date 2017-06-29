-- start_ignore
! rmdir /sys/fs/cgroup/cpu/gpdb;
! rmdir /sys/fs/cgroup/cpuacct/gpdb;
! mkdir /sys/fs/cgroup/cpu/gpdb;
! mkdir /sys/fs/cgroup/cpuacct/gpdb;
-- end_ignore

-- enable resource group and restart cluster.
-- start_ignore
! gpconfig -c gp_resource_manager -v group;
! gpstop -rai;
-- end_ignore

SHOW gp_resource_manager;

-- resource queue statistics should not crash
SELECT * FROM pg_resqueue_status;
SELECT * FROM gp_toolkit.gp_resqueue_status;
SELECT * FROM gp_toolkit.gp_resq_priority_backend;

-- by default admin_group has concurrency set to -1 which leads to
-- very small memory quota for each resgroup slot, correct it.
ALTER RESOURCE GROUP admin_group SET concurrency 2;
