-- ----------------------------------------------------------------------
-- Test: manage resource group in transaction
-- ----------------------------------------------------------------------

--start_ignore
DROP RESOURCE GROUP rg_test_group;
--end_ignore

-- helper view to check the resgroup status
CREATE OR REPLACE VIEW rg_test_monitor AS
	SELECT groupname, concurrency, proposed_concurrency, cpu_rate_limit
	FROM gp_toolkit.gp_resgroup_config
	WHERE groupname='rg_test_group';

-- ----------------------------------------------------------------------
-- Test: new resource group created in transaction then rollback
-- ----------------------------------------------------------------------

-- CREATE then ROLLBACK
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- CREATE, DROP then ROLLBACK
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;

	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- CREATE, ALTER then ROLLBACK
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- CREATE, ALTER, DROP then ROLLBACK
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;

	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- ----------------------------------------------------------------------
-- Test: new resource group created in transaction then commit
-- ----------------------------------------------------------------------

-- CREATE then COMMIT
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;
DROP RESOURCE GROUP rg_test_group;
SELECT * FROM rg_test_monitor;

-- CREATE, DROP then COMMIT
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;

	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;

-- CREATE, ALTER then COMMIT
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;
DROP RESOURCE GROUP rg_test_group;
SELECT * FROM rg_test_monitor;

-- CREATE, ALTER, DROP then COMMIT
BEGIN;
	CREATE RESOURCE GROUP rg_test_group
		WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;

	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;

-- ----------------------------------------------------------------------
-- Test: manage existing resource group in transaction then rollback
-- ----------------------------------------------------------------------

CREATE RESOURCE GROUP rg_test_group
	WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);

-- DROP then ROLLBACK
BEGIN;
	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- ALTER then ROLLBACK
BEGIN;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- ALTER, DROP then ROLLBACK
BEGIN;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;

	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
ROLLBACK;
SELECT * FROM rg_test_monitor;

-- ----------------------------------------------------------------------
-- Test: manage existing resource group in transaction then commit
-- ----------------------------------------------------------------------

-- DROP then COMMIT
BEGIN;
	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;
CREATE RESOURCE GROUP rg_test_group
	WITH (concurrency=10, cpu_rate_limit=10, memory_limit=10);
SELECT * FROM rg_test_monitor;

-- ALTER then COMMIT
BEGIN;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 11;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 11;
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;

-- ALTER, DROP then COMMIT
BEGIN;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 12;
	SELECT * FROM rg_test_monitor;

	ALTER RESOURCE GROUP rg_test_group SET CPU_RATE_LIMIT 12;
	SELECT * FROM rg_test_monitor;

	DROP RESOURCE GROUP rg_test_group;
	SELECT * FROM rg_test_monitor;
COMMIT;
SELECT * FROM rg_test_monitor;

-- ----------------------------------------------------------------------
-- Test: create/alter/drop a resource group in subtransaction
-- ----------------------------------------------------------------------

-- CREATE RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
	SAVEPOINT rg_savepoint;
	CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
	ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;

-- ALTER RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
	CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
	SAVEPOINT rg_savepoint;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 10;
	ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;

-- DROP RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
	CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
	SAVEPOINT rg_savepoint;
	DROP RESOURCE GROUP rg_test_group;
	ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;

-- cleanup
DROP VIEW rg_test_monitor;
