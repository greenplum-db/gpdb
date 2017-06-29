-- start_ignore
! rmdir /sys/fs/cgroup/cpu/gpdb/*/;
! rmdir /sys/fs/cgroup/cpuacct/gpdb/*/;
! rmdir /sys/fs/cgroup/cpu/gpdb;
! rmdir /sys/fs/cgroup/cpuacct/gpdb;
-- end_ignore

-- gpdb top group is not created
! gpconfig -c gp_resource_manager -v group;

-- start_ignore
! mkdir /sys/fs/cgroup/cpu/gpdb;
! mkdir /sys/fs/cgroup/cpuacct/gpdb;
! chmod 644 /sys/fs/cgroup/cpu/gpdb;
-- end_ignore

-- gpdb directory should have rwx permission
! gpconfig -c gp_resource_manager -v group;

-- start_ignore
! chmod 755 /sys/fs/cgroup/cpu/gpdb;
! chmod 444 /sys/fs/cgroup/cpu/gpdb/cgroup.procs;
! chmod 444 /sys/fs/cgroup/cpu/gpdb/cpu.cfs_quota_us;
! chmod 244 /sys/fs/cgroup/cpu/gpdb/cpu.cfs_period_us;
! chmod 244 /sys/fs/cgroup/cpuacct/gpdb/cpuacct.usage;
-- end_ignore

-- cgroup.procs should have writepermission
-- cpu.cfs_quota_us should have write permission
-- cpu.cfs_period_us should have read permission
-- cpuacct.usage should have read permission
! gpconfig -c gp_resource_manager -v group;
