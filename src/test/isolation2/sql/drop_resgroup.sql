1: SET ROLE TO r1;

0: ALTER ROLE r1 RESOURCE GROUP none;
0: DROP RESOURCE GROUP g1;

2: SET ROLE TO r2;

0: ALTER ROLE r2 RESOURCE GROUP none;
0: DROP RESOURCE GROUP g2;

! ls -d /sys/fs/cgroup/cpu/gpdb/*/;
! ls -d /sys/fs/cgroup/cpuacct/gpdb/*/;
