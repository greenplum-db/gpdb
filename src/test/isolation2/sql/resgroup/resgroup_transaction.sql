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
-- Test: create/alter/drop a resource group in transaction block
-- ----------------------------------------------------------------------

-- CREATE RESOURCE GROUP cannot run inside a transaction block
BEGIN;
	CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
END;
SELECT * FROM rg_test_monitor;

-- ALTER RESOURCE GROUP cannot run inside a transaction block
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
BEGIN;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 10;
END;
SELECT * FROM rg_test_monitor;

-- DROP RESOURCE GROUP cannot run inside a transaction block
BEGIN;
	DROP RESOURCE GROUP rg_test_group;
END;
SELECT * FROM rg_test_monitor;

DROP RESOURCE GROUP rg_test_group;


-- ----------------------------------------------------------------------
-- Test: create/alter/drop a resource group and DML in transaction block
-- ----------------------------------------------------------------------

-- CREATE RESOURCE GROUP cannot run inside a transaction block
BEGIN;
	SELECT 1;
	CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
END;
SELECT * FROM rg_test_monitor;

-- ALTER RESOURCE GROUP cannot run inside a transaction block
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
BEGIN;
	SELECT 1;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 10;
END;
SELECT * FROM rg_test_monitor;

-- DROP RESOURCE GROUP cannot run inside a transaction block
BEGIN;
	SELECT 1;
	DROP RESOURCE GROUP rg_test_group;
END;
SELECT * FROM rg_test_monitor;

DROP RESOURCE GROUP rg_test_group;


-- ----------------------------------------------------------------------
-- Test: create/alter/drop a resource group in subtransaction
-- ----------------------------------------------------------------------

-- CREATE RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
	SAVEPOINT rg_savepoint;
	CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
	ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;
SELECT * FROM rg_test_monitor;

-- ALTER RESOURCE GROUP cannot run inside a subtransaction
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
BEGIN;
	SAVEPOINT rg_savepoint;
	ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 10;
	ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;
SELECT * FROM rg_test_monitor;

-- DROP RESOURCE GROUP cannot run inside a subtransaction
BEGIN;
	SAVEPOINT rg_savepoint;
	DROP RESOURCE GROUP rg_test_group;
	ROLLBACK TO SAVEPOINT rg_savepoint;
ABORT;
SELECT * FROM rg_test_monitor;

DROP RESOURCE GROUP rg_test_group;

-- ----------------------------------------------------------------------
-- Test: create/alter/drop a resource group in function call
-- ----------------------------------------------------------------------

CREATE OR REPLACE FUNCTION rg_create_func() RETURNS VOID
AS $$ CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5) $$
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION rg_alter_func() RETURNS VOID
AS $$ ALTER RESOURCE GROUP rg_test_group SET CONCURRENCY 10 $$
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION rg_drop_func() RETURNS VOID
AS $$ DROP RESOURCE GROUP rg_test_group $$
LANGUAGE SQL;

-- CREATE RESOURCE GROUP cannot run inside a function call
SELECT * FROM rg_create_func();
SELECT * FROM rg_test_monitor;

-- ALTER RESOURCE GROUP cannot run inside a function call
CREATE RESOURCE GROUP rg_test_group WITH (cpu_rate_limit=5, memory_limit=5);
SELECT * FROM rg_alter_func();
SELECT * FROM rg_test_monitor;

-- DROP RESOURCE GROUP cannot run inside a function call
SELECT * FROM rg_drop_func();
SELECT * FROM rg_test_monitor;

DROP RESOURCE GROUP rg_test_group;
DROP FUNCTION rg_create_func();
DROP FUNCTION rg_alter_func();
DROP FUNCTION rg_drop_func();

-- ----------------------------------------------------------------------
-- Test: Create index concurrently under the control of resource group
-- ----------------------------------------------------------------------
-- GPDB will commit a transaction and restart a transaction when defining
-- index concurrently. We need to verify resource group can handle this
-- case.
CREATE TABLE concur_table (f1 text, f2 text, dk text) distributed by (dk);
SET gp_create_index_concurrently = TRUE;
CREATE INDEX CONCURRENTLY idx_concur_table ON concur_table(f2,f1);
DROP INDEX idx_concur_table;
DROP TABLE concur_table;

-- cleanup
DROP VIEW rg_test_monitor;
