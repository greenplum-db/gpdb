-- ----------------------------------------------------------------------
-- Test: assign/alter a role to a resource group
-- ----------------------------------------------------------------------

DROP ROLE IF EXISTS rg_test_role;

-- positive
CREATE ROLE rg_test_role;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
CREATE ROLE rg_test_role_super SUPERUSER;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role_super';
ALTER ROLE rg_test_role_super NOSUPERUSER;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role_super';
ALTER ROLE rg_test_role_super SUPERUSER;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role_super';

ALTER ROLE rg_test_role RESOURCE GROUP none;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role_super RESOURCE GROUP none;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role_super';

ALTER ROLE rg_test_role RESOURCE GROUP default_group;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role_super RESOURCE GROUP admin_group;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role_super';


-- negative
ALTER ROLE rg_test_role RESOURCE GROUP non_exist_group;
ALTER ROLE rg_test_role RESOURCE GROUP admin_group;

CREATE ROLE rg_test_role1 RESOURCE GROUP non_exist_group;
-- nonsuper user should not be assigned to admin group
CREATE ROLE rg_test_role1 RESOURCE GROUP admin_group;

-- cleanup
DROP ROLE rg_test_role;
DROP ROLE rg_test_role_super;

-- ----------------------------------------------------------------------
-- Test: create/drop a resource group
-- ----------------------------------------------------------------------

--start_ignore
DROP RESOURCE GROUP rg_test_group;
--end_ignore

SELECT * FROM gp_toolkit.gp_resgroup_config;

-- negative

-- can't create the reserved resource groups
CREATE RESOURCE GROUP default_group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP admin_group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP none WITH (cpu_rate_limit=10, memory_limit=10);
-- multiple resource groups can't share the same name
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10);
DROP RESOURCE GROUP rg_test_group;
-- must specify cpu_rate_limit or cpuset
CREATE RESOURCE GROUP rg_test_group WITH (memory_limit=10);
-- can't specify the resource limit type multiple times
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=5, memory_limit=5, concurrency=1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5, cpu_rate_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5, memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5, memory_shared_quota=70, memory_shared_quota=80);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0', cpuset='0', memory_limit=5);
-- can't specify both cpu_rate_limit and cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, cpuset='0', memory_limit=5);
-- can't specify invalid cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset=',', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='-', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='a', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='12a', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0-,', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='-1', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='3-1', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset=' 0 ', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='4;a', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='-;4', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset=';5', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='5;', memory_limit=5);

---- suppose the core numbered 1024 is not exist
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='1024', memory_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,', memory_limit=5);
-- can't alter to invalid cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0', memory_limit=5);
ALTER RESOURCE GROUP rg_test_group set CPUSET '';
ALTER RESOURCE GROUP rg_test_group set CPUSET ',';
ALTER RESOURCE GROUP rg_test_group set CPUSET '-';
ALTER RESOURCE GROUP rg_test_group set CPUSET 'a';
ALTER RESOURCE GROUP rg_test_group set CPUSET '12a';
ALTER RESOURCE GROUP rg_test_group set CPUSET '0-';
ALTER RESOURCE GROUP rg_test_group set CPUSET '-1';
ALTER RESOURCE GROUP rg_test_group set CPUSET '3-1';
ALTER RESOURCE GROUP rg_test_group set CPUSET ' 0 ';
ALTER RESOURCE GROUP rg_test_group set CPUSET '4;a';
ALTER RESOURCE GROUP rg_test_group set CPUSET '-;4';
ALTER RESOURCE GROUP rg_test_group set CPUSET ';5';
ALTER RESOURCE GROUP rg_test_group set CPUSET '5;';
ALTER RESOURCE GROUP rg_test_group set CPUSET ';';
ALTER RESOURCE GROUP rg_test_group set CPUSET '1;2;';
ALTER RESOURCE GROUP rg_test_group set CPUSET '1;2;3';
---- suppose the core numbered 1024 is not exist
ALTER RESOURCE GROUP rg_test_group set CPUSET '1024';
ALTER RESOURCE GROUP rg_test_group set CPUSET '0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,';
DROP RESOURCE GROUP rg_test_group;
-- can't drop non-exist resource group
DROP RESOURCE GROUP non_exist_group;
-- can't drop reserved resource groups
DROP RESOURCE GROUP default_group;
DROP RESOURCE GROUP admin_group;
DROP RESOURCE GROUP none;

-- positive
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10);
SELECT groupname,concurrency,cpu_rate_limit,memory_limit,memory_shared_quota,memory_spill_ratio FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpuset='0', memory_limit=10, memory_shared_quota=70, memory_spill_ratio=30);
SELECT groupname,concurrency,cpu_rate_limit,memory_limit,memory_shared_quota,memory_spill_ratio FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10);
SELECT groupname,concurrency,cpu_rate_limit,memory_limit,memory_shared_quota,memory_spill_ratio FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0');
SELECT groupname,concurrency,cpu_rate_limit,memory_limit,memory_shared_quota,memory_spill_ratio FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0;0-1');
SELECT groupname,concurrency,cpu_rate_limit,memory_limit,memory_shared_quota,memory_spill_ratio
FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
-- ----------------------------------------------------------------------
-- Test: boundary check in create resource group syntax
-- ----------------------------------------------------------------------

-- negative: cpu_rate_limit & memory_limit should be in [1, 100]
-- if cpu_rate_limit equals -1, it will not be involved in sum
CREATE RESOURCE GROUP rg_test_group1 WITH (memory_limit=10, cpuset='0');
CREATE RESOURCE GROUP rg_test_group2 WITH (cpu_rate_limit=60, memory_limit=10);
CREATE RESOURCE GROUP rg_test_group3 WITH (cpu_rate_limit=1, memory_limit=10);
DROP RESOURCE GROUP rg_test_group1;
DROP RESOURCE GROUP rg_test_group2;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=61, memory_limit=10);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=91);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=0, memory_limit=10);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=-1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=0.9);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=1.9);
-- negative: concurrency should be in [1, max_connections]
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=-1, cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=26, cpu_rate_limit=10, memory_limit=10);
-- negative: memory_auditor should be 'vmtracker' or 'cgroup'
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=0, cpu_rate_limit=10, memory_limit=10, memory_auditor="randomtext");
-- negative: concurrency should be zero for cgroup audited resource group
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=10, memory_limit=10, memory_auditor="cgroup");
-- negative: the cores of cpuset in different groups mustn't overlap
CREATE RESOURCE GROUP rg_test_group1 WITH (cpuset='0', memory_limit=10);
CREATE RESOURCE GROUP rg_test_group2 WITH (cpuset='0', memory_limit=10);
DROP RESOURCE GROUP rg_test_group1;

-- memory_spill_ratio range is [0, 100]
-- no limit on the sum of memory_shared_quota and memory_spill_ratio
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=10, memory_spill_ratio=0);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=50, memory_spill_ratio=51);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=10, memory_spill_ratio=-1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=-1, memory_spill_ratio=10);

-- positive: cpu_rate_limit & memory_limit should be in [1, 100]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=60, memory_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=60);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=1, memory_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=1);
DROP RESOURCE GROUP rg_test_group;
-- positive: concurrency should be in [0, max_connections]
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=0, cpu_rate_limit=10, memory_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=10, memory_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=25, cpu_rate_limit=10, memory_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg1_test_group WITH (concurrency=1, cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=50, memory_limit=50);
DROP RESOURCE GROUP rg1_test_group;
DROP RESOURCE GROUP rg2_test_group;
CREATE RESOURCE GROUP rg1_test_group WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=40, memory_limit=40);
DROP RESOURCE GROUP rg1_test_group;
DROP RESOURCE GROUP rg2_test_group;
CREATE RESOURCE GROUP rg1_test_group WITH (concurrency=1, cpu_rate_limit=30, memory_limit=30);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=30, memory_limit=30);
DROP RESOURCE GROUP rg1_test_group;
DROP RESOURCE GROUP rg2_test_group;
-- positive: concurrency should be zero for cgroup audited resource group
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=0, cpu_rate_limit=10, memory_limit=10, memory_auditor="cgroup");
DROP RESOURCE GROUP rg_test_group;

-- memory_spill_ratio range is [0, 100]
-- no limit on the sum of memory_shared_quota and memory_spill_ratio
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=0, memory_spill_ratio=1);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=50, memory_spill_ratio=50);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=0, memory_spill_ratio=100);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_shared_quota=99, memory_spill_ratio=1);
DROP RESOURCE GROUP rg_test_group;

-- negative: memory_spill_ratio does not accept out of range percentage values
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=-1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=101);

-- negative: memory_spill_ratio does not accept string values
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio='0');
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio='10');

-- negative: memory_spill_ratio does not accept float values
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=10.5);

-- negative: when memory_limit is unlimited memory_spill_ratio must be set to 0
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=0, memory_spill_ratio=10);

-- positive
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=0, memory_spill_ratio=0);
DROP RESOURCE GROUP rg_test_group;

-- ----------------------------------------------------------------------
-- Test: alter a resource group
-- ----------------------------------------------------------------------
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);

-- ALTER RESOURCE GROUP SET CONCURRENCY N
-- negative: concurrency should be in [1, max_connections]
ALTER RESOURCE GROUP admin_group SET CONCURRENCY 0;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY -1;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 26;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY -0.5;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 0.5;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY a;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 'abc';
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY '1';
-- positive: concurrency should be in [1, max_connections]
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 0;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 1;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 2;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 25;

-- ALTER RESOURCE GROUP SET CPU_RATE_LIMIT VALUE
-- negative: cpu_rate_limit & memory_limit should be in [1, 100]
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT -0.1;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT -1;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.7;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 1.7;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 61;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT a;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 'abc';
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 20%;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.2%;
-- positive: cpu_rate_limit & memory_limit should be in [1, 100]
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 1;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 2;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 60;
DROP RESOURCE GROUP rg_test_group;
-- positive: total cpu_rate_limit & memory_limit should be in [1, 100]
CREATE RESOURCE GROUP rg1_test_group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP rg2_test_group WITH (cpu_rate_limit=10, memory_limit=10);
ALTER RESOURCE GROUP rg1_test_group SET CPU_RATE_LIMIT 50;
ALTER RESOURCE GROUP rg1_test_group SET CPU_RATE_LIMIT 40;
ALTER RESOURCE GROUP rg2_test_group SET CPU_RATE_LIMIT 20;
ALTER RESOURCE GROUP rg1_test_group SET CPU_RATE_LIMIT 30;
ALTER RESOURCE GROUP rg2_test_group SET CPU_RATE_LIMIT 30;
DROP RESOURCE GROUP rg1_test_group;
DROP RESOURCE GROUP rg2_test_group;
-- positive: cpuset and cpu_rate_limit are exclusive,
-- if cpu_rate_limit is set, cpuset is empty
-- if cpuset is set, cpuset is -1
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10);
ALTER RESOURCE GROUP rg_test_group SET CPUSET '0';
SELECT groupname,cpu_rate_limit,memory_limit,cpuset FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 10;
SELECT groupname,cpu_rate_limit,memory_limit,cpuset FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;

CREATE RESOURCE GROUP cgroup_audited_group WITH (concurrency=0, cpu_rate_limit=10, memory_limit=10, memory_auditor="cgroup");
-- negative: memory_auditor cannot be altered
ALTER RESOURCE GROUP cgroup_audited_group SET MEMORY_AUDITOR "default";
-- negative: concurrency should be zero for cgroup audited resource group
ALTER RESOURCE GROUP cgroup_audited_group SET CONCURRENCY 10;
-- negative: role should not be assigned to a cgroup audited resource group
CREATE ROLE cgroup_audited_role RESOURCE GROUP cgroup_audited_group;
DROP RESOURCE GROUP cgroup_audited_group;

-- positive: memory_spill_ratio accepts integer values
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=20);
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio 10;
DROP RESOURCE GROUP rg_test_group;

-- negative: memory_spill_ratio only accepts integer values
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=20);
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio '10';
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio 10.5;
DROP RESOURCE GROUP rg_test_group;

-- negative: memory_spill_ratio does not accept out of range values
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=20);
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio -1;
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio 101;
DROP RESOURCE GROUP rg_test_group;

-- positive: memory_limit can be altered to unlimited if memory_spill_ratio is 0
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=0);
ALTER RESOURCE GROUP rg_test_group SET memory_limit 0;
DROP RESOURCE GROUP rg_test_group;

-- negative: memory_spill_ratio can only be set to 0 if memory_limit is unlimited
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=0, memory_spill_ratio=0);
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio 10;
DROP RESOURCE GROUP rg_test_group;

-- positive: memory_spill_ratio accepts a percentage value only if
-- memory_limit is limited
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=0);
ALTER RESOURCE GROUP rg_test_group SET memory_spill_ratio 10;
DROP RESOURCE GROUP rg_test_group;

-- negative: memory_limit must be limited if memory_spill_ratio > 0
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=10, memory_limit=10, memory_spill_ratio=10);
ALTER RESOURCE GROUP rg_test_group SET memory_limit 0;
DROP RESOURCE GROUP rg_test_group;
