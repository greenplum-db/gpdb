/* contrib/gp_partition_maint/gp_partition_maint--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_partition_maint" to load this file. \quit

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_rank(rp regclass)
RETURNS int
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_bound_value(rp regclass, bound_type text)
RETURNS text AS $$
DECLARE
	v_relpartbound text;
	v_bound_value text;
	v_parent_table regclass;
	v_nkeys int;
BEGIN
-- Check if the given table is a non-default child range partition
SELECT inhparent INTO v_parent_table
FROM pg_inherits
WHERE inhrelid = rp;

IF v_parent_table IS NULL THEN
	RETURN NULL;
END IF;

-- Check if the parent table is partitioned by a single key
SELECT partnatts INTO v_nkeys
FROM pg_partitioned_table
WHERE partrelid = v_parent_table;

IF v_nkeys IS NOT NULL AND v_nkeys != 1 THEN
	RETURN NULL;
END IF;

-- Get the partition bounds
SELECT pg_get_expr(relpartbound, oid) INTO v_relpartbound
FROM pg_class
WHERE oid = rp;

-- Parse the bound value from relpartbound
IF lower(bound_type) = 'from' THEN
	SELECT (regexp_matches(v_relpartbound, 'FOR VALUES FROM \((.+)\) TO \((.+)\)'))[1] INTO v_bound_value;
ELSIF lower(bound_type) = 'to' THEN
	SELECT (regexp_matches(v_relpartbound, 'FOR VALUES FROM \((.+)\) TO \((.+)\)'))[2] INTO v_bound_value;
ELSE
	RAISE EXCEPTION 'Invalid bound type: %', bound_type;
END IF;

RETURN v_bound_value;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_range_from(rp regclass)
RETURNS text AS $$
SELECT pg_partition_bound_value(rp, 'from');
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_range_to(rp regclass)
RETURNS text AS $$
SELECT pg_partition_bound_value(rp, 'to');
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_isdefault(relid regclass)
RETURNS BOOLEAN AS $$
DECLARE
boundspec TEXT;
BEGIN
-- Get the partition bound definition for the relation
SELECT pg_get_expr(relpartbound, oid) INTO boundspec
FROM pg_catalog.pg_class
WHERE oid = relid;

-- If partition_def is null, the relation is not a partition at all
IF boundspec IS NULL THEN
RETURN FALSE;
END IF;

-- Check if the partition bound spec exactly matches 'DEFAULT'
RETURN boundspec = 'DEFAULT';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_parent_partstrat(childrelname text)
RETURNS "char" AS $$
SELECT part.partstrat
FROM pg_catalog.pg_inherits i
JOIN pg_catalog.pg_class c ON i.inhrelid = c.oid
JOIN pg_catalog.pg_partitioned_table part ON part.partrelid = i.inhparent
WHERE c.relname = childrelname;
$$
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_lowest_child(rp regclass)
RETURNS regclass
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

CREATE OR REPLACE FUNCTION @extschema@.pg_partition_highest_child(rp regclass)
RETURNS regclass
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;
