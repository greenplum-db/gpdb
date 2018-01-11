--
-- ORCA tests which require gp_fault_injector
--

CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

CREATE SCHEMA gporca_faults;
SET search_path = gporca_faults, public;

CREATE TABLE foo (a int, b int);
INSERT INTO foo VALUES (1,1);

-- test interruption requests to optimization
select gp_inject_fault('opt_relcache_translator_catalog_access', 'reset', 1);
select gp_inject_fault('opt_relcache_translator_catalog_access', 'interrupt', 1);
select count(*) from foo;

-- Ensure that ORCA is not called on any process other than the master QD
CREATE FUNCTION func1_nosql_vol(x int) RETURNS int AS $$
BEGIN
  RETURN $1 +1;
END
$$ LANGUAGE plpgsql VOLATILE;

-- Query that runs the function on an additional non-QD master slice
-- Include the EXPLAIN to ensure that this happens in the plan.
EXPLAIN SELECT * FROM func1_nosql_vol(5), foo;

select gp_inject_fault('opt_relcache_translator_catalog_access', 'reset', 1);
select gp_inject_fault('opt_relcache_translator_catalog_access', 'interrupt', 1);
SELECT * FROM func1_nosql_vol(5), foo;

-- The fault should *not* be hit above when optimizer = off, to reset it now.
SELECT gp_inject_fault('opt_relcache_translator_catalog_access', 'reset', 1);
