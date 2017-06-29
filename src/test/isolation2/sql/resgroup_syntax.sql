-- ----------------------------------------------------------------------
-- Test: assign/alter a role to a resource group
-- ----------------------------------------------------------------------

--start_ignore
DROP ROLE IF EXISTS rg_test_role;
--end_ignore
CREATE ROLE rg_test_role;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role RESOURCE GROUP non_exist_group;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role RESOURCE GROUP admin_group;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role RESOURCE GROUP none;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';

DROP ROLE rg_test_role;
CREATE ROLE rg_test_role SUPERUSER;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role RESOURCE GROUP default_group;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';
ALTER ROLE rg_test_role RESOURCE GROUP none;
SELECT rolresgroup FROM pg_authid WHERE rolname = 'rg_test_role';

DROP ROLE rg_test_role;
CREATE ROLE rg_test_role RESOURCE GROUP non_exist_group;

-- nonsuper user should not be assigned to admin group
CREATE ROLE rg_test_role RESOURCE GROUP admin_group;


-- ----------------------------------------------------------------------
-- Test: create/drop a resource group
-- ----------------------------------------------------------------------

--start_ignore
DROP RESOURCE GROUP rg_test_group;
--end_ignore

-- can't drop non-exist resource group
DROP RESOURCE GROUP rg_test_group;

-- can't create the reserved resource groups
CREATE RESOURCE GROUP default_group WITH (concurrency=1, cpu_rate_limit=.5, memory_limit=.5, memory_redzone_limit=.7);
CREATE RESOURCE GROUP admin_group WITH (concurrency=1, cpu_rate_limit=.5, memory_limit=.5, memory_redzone_limit=.7);
CREATE RESOURCE GROUP none WITH (concurrency=1, cpu_rate_limit=.5, memory_limit=.5, memory_redzone_limit=.7);

-- must specify both memory_limit and cpu_rate_limit
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, memory_limit=.5, memory_redzone_limit=.7);
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.5, memory_redzone_limit=.7);

CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.5, memory_limit=.6, memory_redzone_limit=.7);
SELECT groupname,concurrency,proposed_concurrency,cpu_rate_limit,memory_limit,proposed_memory_limit,memory_redzone_limit FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';

-- multiple resource groups can't share the same name
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7);

-- cpu_rate_limit/memory_limit range is (0.01, 1)
CREATE RESOURCE GROUP rg2_test_group WITH (cpu_rate_limit=.5, memory_limit=.05);
CREATE RESOURCE GROUP rg2_test_group WITH (cpu_rate_limit=.05, memory_limit=.5);
CREATE RESOURCE GROUP rg2_test_group WITH (cpu_rate_limit=.01, memory_limit=.05);
CREATE RESOURCE GROUP rg2_test_group WITH (cpu_rate_limit=.05, memory_limit=.01);

-- can't specify the resource limit type multiple times
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7, concurrency=1);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7, cpu_rate_limit=.05);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7, memory_limit=.05);
CREATE RESOURCE GROUP rg2_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7, memory_redzone_limit=.8);

DROP RESOURCE GROUP rg_test_group;

-- can't drop reserved resource groups
DROP RESOURCE GROUP default_group;
DROP RESOURCE GROUP admin_group;
DROP RESOURCE GROUP none;

-- can't drop non-exist resource group
DROP RESOURCE GROUP rg_non_exist_group;

-- ----------------------------------------------------------------------
-- Test: alter a resource group
-- ----------------------------------------------------------------------
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7);

-- ALTER RESOURCE GROUP SET CONCURRENCY N
-- negative
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY -2;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY -0.5;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 0.5;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY a;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 'abc';
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY '1';
-- positive 
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY -1;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 1;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 2;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 1000;

-- ALTER RESOURCE GROUP SET CPU_RATE_LIMIT VALUE
-- negative
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT -0.1;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 2;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.7;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.01;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT a;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 'abc';
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 20%;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.2%;
-- positive
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.1;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.5;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.6;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.6;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.1;
ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 0.02;

DROP RESOURCE GROUP rg_test_group;

-- ----------------------------------------------------------------------
-- Test: create/alter/drop a resource group in subtransaction
-- ----------------------------------------------------------------------

-- CREATE RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
SAVEPOINT rg_savepoint;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7);
ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;

-- ALTER RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7);
SAVEPOINT rg_savepoint;
ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 10;
ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;

-- DROP RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=1, cpu_rate_limit=.05, memory_limit=.05, memory_redzone_limit=.7);
SAVEPOINT rg_savepoint;
DROP RESOURCE GROUP rg_test_group;
ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;
