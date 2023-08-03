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

--cannot create or put role in system_group, should error out
CREATE ROLE r_test_system_group RESOURCE GROUP system_group;
CREATE ROLE r_test_system_group;
ALTER ROLE r_test_system_group RESOURCE GROUP system_group;

CREATE ROLE rg_test_role1 RESOURCE GROUP non_exist_group;
-- nonsuper user should not be assigned to admin group
CREATE ROLE rg_test_role1 RESOURCE GROUP admin_group;

-- cleanup
DROP ROLE rg_test_role;
DROP ROLE rg_test_role_super;
DROP ROLE r_test_system_group;

-- ----------------------------------------------------------------------
-- Test: create/drop a resource group
-- ----------------------------------------------------------------------

--start_ignore
DROP RESOURCE GROUP rg_test_group;
--end_ignore

SELECT * FROM gp_toolkit.gp_resgroup_config;

-- negative

-- can't create the reserved resource groups
CREATE RESOURCE GROUP default_group WITH (cpu_max_percent=10);
CREATE RESOURCE GROUP admin_group WITH (cpu_max_percent=10);
CREATE RESOURCE GROUP none WITH (cpu_max_percent=10);

-- multiple resource groups can't share the same name
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10);
DROP RESOURCE GROUP rg_test_group;

-- can't specify the resource limit type multiple times
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_max_percent=5, concurrency=1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=5, cpu_max_percent=5);
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0', cpuset='0');

-- can't specify both cpu_max_percent and cpuset
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=5, cpuset='0');

-- cpu_weight can't be negative value
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=5, cpu_weight=-100);

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
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10);
SELECT groupname,concurrency,cpu_max_percent, cpu_weight FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpuset='0');
SELECT groupname,concurrency,cpu_max_percent, cpu_weight FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=500);
SELECT groupname,concurrency,cpu_max_percent, cpu_weight FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=-1, cpu_weight=500);
SELECT groupname,concurrency,cpu_max_percent, cpu_weight FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0', cpu_weight=500);
SELECT groupname,concurrency,cpu_max_percent, cpu_weight FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0');
SELECT groupname,concurrency,cpu_max_percent,cpu_weight FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpuset='0;0-1');
SELECT groupname,concurrency,cpu_max_percent,cpu_weight,cpuset
FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;
-- ----------------------------------------------------------------------
-- Test: boundary check in create resource group syntax
-- ----------------------------------------------------------------------

-- negative: cpu_max_percent should be in [1, 100]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=101);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=0);

-- negative: concurrency should be in [1, max_connections]
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=-1, cpu_max_percent=10);
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=26, cpu_max_percent=10);

-- negative: the cores of cpuset in different groups mustn't overlap
CREATE RESOURCE GROUP rg_test_group1 WITH (cpuset='0');
CREATE RESOURCE GROUP rg_test_group2 WITH (cpuset='0');
DROP RESOURCE GROUP rg_test_group1;

-- negative: cpu_weight should be in [1, 500]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=0);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=-1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=-1024);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=501);

-- positive: cpu_max_percent should be in [1, 100]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=60);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=1);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10);
DROP RESOURCE GROUP rg_test_group;

-- positive: cpu_weight should be in [1, 500]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=100);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, cpu_weight=500);
DROP RESOURCE GROUP rg_test_group;

-- positive: concurrency should be in [0, max_connections]
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=0, cpu_max_percent=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_max_percent=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=25, cpu_max_percent=10);
DROP RESOURCE GROUP rg_test_group;
CREATE RESOURCE GROUP rg1_test_group WITH (concurrency=1, cpu_max_percent=10);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_max_percent=500);
DROP RESOURCE GROUP rg1_test_group;
DROP RESOURCE GROUP rg2_test_group;

-- positive: min_cost should be in [0,INT32_MAX]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, min_cost=0);
CREATE RESOURCE GROUP rg1_test_group WITH (cpu_max_percent=10, min_cost=2147483647);
ALTER RESOURCE GROUP rg_test_group SET min_cost 2147483647;
ALTER RESOURCE GROUP rg1_test_group SET min_cost 0;
DROP RESOURCE GROUP rg_test_group;
DROP RESOURCE GROUP rg1_test_group;

-- negative: min_cost should be in [0,INT32_MAX]
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, min_cost=-1);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, min_cost=2147483648);
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, min_cost=0);
ALTER RESOURCE GROUP rg_test_group SET min_cost -1;
ALTER RESOURCE GROUP rg_test_group SET min_cost 2147483648;
DROP RESOURCE GROUP rg_test_group;

--
-- ----------------------------------------------------------------------
-- Test: alter a resource group
-- ----------------------------------------------------------------------
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=5);

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

-- ALTER RESOURCE GROUP SET cpu_max_percent VALUE
-- negative: cpu_max_percent should be in [1, 100]
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent -0.1;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent -1;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 0;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 0.7;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 1.7;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 61;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent a;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 'abc';
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 20%;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 0.2%;
-- positive: cpu_max_percent should be in [1, 100]
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 1;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 2;
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 60;
DROP RESOURCE GROUP rg_test_group;

-- positive: cpuset and cpu_max_percent are exclusive,
-- if cpu_max_percent is set, cpuset is empty
-- if cpuset is set, cpuset is -1
CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10);
ALTER RESOURCE GROUP rg_test_group SET CPUSET '0';
SELECT groupname,cpu_max_percent,cpuset FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 10;
SELECT groupname,cpu_max_percent,cpuset FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;

CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10, concurrency=5);
CREATE ROLE rg_test_role RESOURCE GROUP rg_test_group;
SET ROLE rg_test_role;
CREATE TABLE rg_test_group_table(a int);
SELECT count(*) FROM rg_test_group_table;
SELECT is_session_in_group(pg_backend_pid(), 'rg_test_group');
RESET ROLE;
SELECT count(*) FROM rg_test_group_table;
SELECT is_session_in_group(pg_backend_pid(), 'admin_group');
DROP TABLE rg_test_group_table;
DROP ROLE rg_test_role;
DROP RESOURCE GROUP rg_test_group;

-- test set cpu_max_percent to high value when gp_resource_group_cpu_limit is low
-- start_ignore
!\retcode gpconfig -c gp_resource_group_cpu_limit -v 0.5;
!\retcode gpstop -ari;
-- end_ignore
0: CREATE RESOURCE GROUP rg_test_group WITH (cpu_max_percent=10);
0: ALTER RESOURCE GROUP rg_test_group SET cpu_max_percent 100;
0: DROP RESOURCE GROUP rg_test_group;
-- start_ignore
!\retcode gpconfig -c gp_resource_group_cpu_limit -v 0.9;
!\retcode gpstop -ari;
-- end_ignore

