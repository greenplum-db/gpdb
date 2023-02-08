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
CREATE RESOURCE GROUP default_group WITH (cpu_hard_quota_limit=10);
CREATE RESOURCE GROUP admin_group WITH (cpu_hard_quota_limit=10);
CREATE RESOURCE GROUP none WITH (cpu_hard_quota_limit=10);

-- multiple resource groups can't share the same name
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10);
DROP RESOURCE GROUP rg_test_group;

-- can't specify the resource limit type multiple times
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_hard_quota_limit=5, concurrency=1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=5, cpu_hard_quota_limit=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0', cpuset='0');

-- can't specify both cpu_hard_quota_limit and cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=5, cpuset='0');

-- cpu_soft_priority can't be negative value
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=5, cpu_soft_priority=-100);

-- can't specify invalid cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset=',');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='-');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='a');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='12a');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0-,');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='-1');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='3-1');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset=' 0 ');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='4;a');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='-;4');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset=';5');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='5;');

---- suppose the core numbered 1024 is not exist
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='1024');
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,');
-- can't alter to invalid cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0');
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
DROP RESOURCE GROUP system_group;
DROP RESOURCE GROUP none;

-- positive
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10);
SELECT groupname,concurrency,cpu_hard_quota_limit, cpu_soft_priority FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpuset='0');
SELECT groupname,concurrency,cpu_hard_quota_limit, cpu_soft_priority FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=500);
SELECT groupname,concurrency,cpu_hard_quota_limit, cpu_soft_priority FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=-1, cpu_soft_priority=500);
SELECT groupname,concurrency,cpu_hard_quota_limit, cpu_soft_priority FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0', cpu_soft_priority=500);
SELECT groupname,concurrency,cpu_hard_quota_limit, cpu_soft_priority FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0');
SELECT groupname,concurrency,cpu_hard_quota_limit,cpu_soft_priority FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0;0-1');
SELECT groupname,concurrency,cpu_hard_quota_limit,cpu_soft_priority,cpuset
FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
-- ----------------------------------------------------------------------
-- Test: boundary check in create resource group syntax
-- ----------------------------------------------------------------------

-- negative: cpu_hard_quota_limit should be in [1, 100]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=101);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=0);

-- negative: concurrency should be in [1, max_connections]
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=-1, cpu_hard_quota_limit=10);
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=26, cpu_hard_quota_limit=10);

-- negative: the cores of cpuset in different groups mustn't overlap
CREATE RESOURCE GROUP rg_test_group1 WITH (cpuset='0');
CREATE RESOURCE GROUP rg_test_group2 WITH (cpuset='0');
DROP RESOURCE GROUP rg_test_group1;

-- negative: cpu_soft_priority should be in [1, 500]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=0);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=-1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=-1024);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=501);

-- positive: cpu_hard_quota_limit should be in [1, 100]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=60);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=1);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10);
DROP RESOURCE GROUP rg_test_group;

-- positive: cpu_soft_priority should be in [1, 500]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=100);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10, cpu_soft_priority=500);
DROP RESOURCE GROUP rg_test_group;

-- positive: concurrency should be in [0, max_connections]
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=0, cpu_hard_quota_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_hard_quota_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=25, cpu_hard_quota_limit=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg1_test_group WITH (concurrency=1, cpu_hard_quota_limit=10);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_hard_quota_limit=500);
DROP RESOURCE GROUP rg1_test_group;
DROP RESOURCE GROUP rg2_test_group;

-- ----------------------------------------------------------------------
-- Test: alter a resource group
-- ----------------------------------------------------------------------
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=5);

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

-- ALTER RESOURCE GROUP SET cpu_hard_quota_limit VALUE
-- negative: cpu_hard_quota_limit should be in [1, 100]
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit -0.1;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit -1;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 0;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 0.7;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 1.7;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 61;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit a;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 'abc';
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 20%;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 0.2%;
-- positive: cpu_hard_quota_limit should be in [1, 100]
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 1;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 2;
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 60;
DROP RESOURCE GROUP rg_test_group;

-- positive: cpuset and cpu_hard_quota_limit are exclusive,
-- if cpu_hard_quota_limit is set, cpuset is empty
-- if cpuset is set, cpuset is -1
CREATE RESOURCE GROUP rg_test_group WITH (cpu_hard_quota_limit=10);
ALTER RESOURCE GROUP rg_test_group SET CPUSET '0';
SELECT groupname,cpu_hard_quota_limit,cpuset FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
ALTER RESOURCE GROUP rg_test_group SET cpu_hard_quota_limit 10;
SELECT groupname,cpu_hard_quota_limit,cpuset FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
