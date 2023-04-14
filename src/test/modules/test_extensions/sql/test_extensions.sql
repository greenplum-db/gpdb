-- test some errors
CREATE EXTENSION test_ext1;
CREATE EXTENSION test_ext1 SCHEMA test_ext1;
CREATE EXTENSION test_ext1 SCHEMA test_ext;
CREATE SCHEMA test_ext;
CREATE EXTENSION test_ext1 SCHEMA test_ext;

-- finally success
CREATE EXTENSION test_ext1 SCHEMA test_ext CASCADE;

SELECT extname, nspname, extversion, extrelocatable FROM pg_extension e, pg_namespace n WHERE extname LIKE 'test_ext%' AND e.extnamespace = n.oid ORDER BY 1;

CREATE EXTENSION test_ext_cyclic1 CASCADE;

DROP SCHEMA test_ext CASCADE;

CREATE EXTENSION test_ext6;
DROP EXTENSION test_ext6;
CREATE EXTENSION test_ext6;

-- test dropping of member tables that own extensions:
-- this table will be absorbed into test_ext7
create table old_table1 (col1 serial primary key);
create extension test_ext7;
\dx+ test_ext7
alter extension test_ext7 update to '2.0';
\dx+ test_ext7

-- test handling of temp objects created by extensions
create extension test_ext8;

-- \dx+ would expose a variable pg_temp_nn schema name, so we can't use it here
select regexp_replace(pg_describe_object(classid, objid, objsubid),
                      'pg_temp_\d+', 'pg_temp', 'g') as "Object description"
from pg_depend
where refclassid = 'pg_extension'::regclass and deptype = 'e' and
  refobjid = (select oid from pg_extension where extname = 'test_ext8')
order by 1;

-- Should be possible to drop and recreate this extension
drop extension test_ext8;
create extension test_ext8;

select regexp_replace(pg_describe_object(classid, objid, objsubid),
                      'pg_temp_\d+', 'pg_temp', 'g') as "Object description"
from pg_depend
where refclassid = 'pg_extension'::regclass and deptype = 'e' and
  refobjid = (select oid from pg_extension where extname = 'test_ext8')
order by 1;

-- here we want to start a new session and wait till old one is gone
select pg_backend_pid() as oldpid \gset
\c -
do 'declare c int = 0;
begin
  while (select count(*) from pg_stat_activity where pid = '
    :'oldpid'
  ') > 0 loop c := c + 1; perform pg_stat_clear_snapshot(); end loop;
  raise log ''test_extensions looped % times'', c;
end';

-- extension should now contain no temp objects
\dx+ test_ext8

-- dropping it should still work
drop extension test_ext8;

-- Test creation of extension in temporary schema with two-phase commit,
-- which should not work.  This function wrapper is useful for portability.

-- Avoid noise caused by CONTEXT and NOTICE messages including the temporary
-- schema name.
\set SHOW_CONTEXT never
SET client_min_messages TO 'warning';
-- First enforce presence of temporary schema.
CREATE TEMP TABLE test_ext4_tab ();
CREATE OR REPLACE FUNCTION create_extension_with_temp_schema()
  RETURNS VOID AS $$
  DECLARE
    tmpschema text;
    query text;
  BEGIN
    SELECT INTO tmpschema pg_my_temp_schema()::regnamespace;
    query := 'CREATE EXTENSION test_ext4 SCHEMA ' || tmpschema || ' CASCADE;';
    RAISE NOTICE 'query %', query;
    EXECUTE query;
  END; $$ LANGUAGE plpgsql;
BEGIN;
SELECT create_extension_with_temp_schema();
PREPARE TRANSACTION 'twophase_extension';
-- rollback transaction, or further commands will fail with message (at gpdb):
-- "ERROR:  current transaction is aborted, commands ignored until end of transaction block"
ROLLBACK;
-- Clean up
DROP TABLE test_ext4_tab;
DROP FUNCTION create_extension_with_temp_schema();
RESET client_min_messages;
\unset SHOW_CONTEXT

-- It's generally bad style to use CREATE OR REPLACE unnecessarily.
-- Test what happens if an extension does it anyway.
-- Replacing a shell type or operator is sort of like CREATE OR REPLACE;
-- check that too.

CREATE FUNCTION ext_cor_func() RETURNS text
  AS $$ SELECT 'ext_cor_func: original'::text $$ LANGUAGE sql;

CREATE EXTENSION test_ext_cor;  -- fail

SELECT ext_cor_func();

DROP FUNCTION ext_cor_func();

CREATE VIEW ext_cor_view AS
  SELECT 'ext_cor_view: original'::text AS col;

CREATE EXTENSION test_ext_cor;  -- fail

SELECT ext_cor_func();

SELECT * FROM ext_cor_view;

DROP VIEW ext_cor_view;

CREATE TYPE test_ext_type;

CREATE EXTENSION test_ext_cor;  -- fail

DROP TYPE test_ext_type;

-- this makes a shell "point <<@@ polygon" operator too
CREATE OPERATOR @@>> ( PROCEDURE = poly_contain_pt,
  LEFTARG = polygon, RIGHTARG = point,
  COMMUTATOR = <<@@ );

CREATE EXTENSION test_ext_cor;  -- fail

DROP OPERATOR <<@@ (point, polygon);

CREATE EXTENSION test_ext_cor;  -- now it should work

SELECT ext_cor_func();

SELECT * FROM ext_cor_view;

SELECT 'x'::test_ext_type;

SELECT point(0,0) <<@@ polygon(circle(point(0,0),1));

\dx+ test_ext_cor

--
-- CREATE IF NOT EXISTS is an entirely unsound thing for an extension
-- to be doing, but let's at least plug the major security hole in it.
--

CREATE COLLATION ext_cine_coll
  ( LC_COLLATE = "C", LC_CTYPE = "C" );

CREATE EXTENSION test_ext_cine;  -- fail

DROP COLLATION ext_cine_coll;

CREATE MATERIALIZED VIEW ext_cine_mv AS SELECT 11 AS f1;

CREATE EXTENSION test_ext_cine;  -- fail

DROP MATERIALIZED VIEW ext_cine_mv;

CREATE FOREIGN DATA WRAPPER dummy;

CREATE SERVER ext_cine_srv FOREIGN DATA WRAPPER dummy;

CREATE EXTENSION test_ext_cine;  -- fail

DROP SERVER ext_cine_srv;

CREATE SCHEMA ext_cine_schema;

CREATE EXTENSION test_ext_cine;  -- fail

DROP SCHEMA ext_cine_schema;

CREATE SEQUENCE ext_cine_seq;

CREATE EXTENSION test_ext_cine;  -- fail

DROP SEQUENCE ext_cine_seq;

CREATE TABLE ext_cine_tab1 (x int);

CREATE EXTENSION test_ext_cine;  -- fail

DROP TABLE ext_cine_tab1;

CREATE TABLE ext_cine_tab2 AS SELECT 42 AS y;

CREATE EXTENSION test_ext_cine;  -- fail

DROP TABLE ext_cine_tab2;

CREATE EXTENSION test_ext_cine;

\dx+ test_ext_cine

ALTER EXTENSION test_ext_cine UPDATE TO '1.1';

\dx+ test_ext_cine



--
-- Test cases from Issue: https://github.com/greenplum-db/gpdb/issues/6716
--
drop extension if exists gp_inject_fault;
create schema issue6716;
create extension gp_inject_fault with schema issue6716;
select issue6716.gp_inject_fault('issue6716', 'skip', 1);
select issue6716.gp_inject_fault('issue6716', 'reset', 1);
drop extension gp_inject_fault;

--
-- Another test cases for problem https://github.com/greenplum-db/gpdb/issues/6716.
-- Segments of gpdb builed with `--enable-cassert` stops with error like
-- FailedAssertion(""!(stack->state == GUC_SAVE)" at next cases. At gpdb builed
-- without `--enable-cassert` segments won't stop with errors, but there may be
-- incorrect search_path.
--

--
-- create extension in the same schema
--
begin;
set search_path=pg_catalog;
create extension btree_gin;
show search_path;
rollback;

--
-- create extension in the different schema
--
begin;
set search_path=issue6716;
show search_path;
create extension btree_gin with schema pg_catalog;
show search_path;
end;

-- check search_path after transaction commit
show search_path;

drop extension btree_gin;

--
-- Test case for create extension from unpackaged
--

-- Create extension functions at existing schema (issue6716). Code copied from from test_ext_cau--1.0.sql
create function test_func1(a int, b int) returns int
as $$
begin
	return a + b;
end;
$$
LANGUAGE plpgsql;

create function test_func2(a int, b int) returns int
as $$
begin
	return a - b;
end;
$$
LANGUAGE plpgsql;

-- restore search path
reset search_path;

begin;
-- change search_path
set search_path=pg_catalog;
show search_path;

-- create extension in schema issue6716
create extension test_ext_cau with schema issue6716 version '1.1' from unpackaged;

-- check that search path doesn't changed after create extension
show search_path;

-- show that functions belong to schema issue6716 (check that create extension works correctly)
set search_path=issue6716;
\df

SELECT e.extname, ne.nspname AS extschema, p.proname, np.nspname AS proschema
FROM pg_catalog.pg_extension AS e
    INNER JOIN pg_catalog.pg_depend AS d ON (d.refobjid = e.oid)
    INNER JOIN pg_catalog.pg_proc AS p ON (p.oid = d.objid)
    INNER JOIN pg_catalog.pg_namespace AS ne ON (ne.oid = e.extnamespace)
    INNER JOIN pg_catalog.pg_namespace AS np ON (np.oid = p.pronamespace)
WHERE d.deptype = 'e' and e.extname = 'test_ext_cau'
ORDER BY 1, 3;
end;

-- check search_path after transaction commit
show search_path;

reset search_path;
drop function issue6716.test_func1(int,int);
drop extension test_ext_cau;

--
-- check that alter extension (with search_path is set) won't fail (on gpdb builded with --enable-cassert)
--
create extension test_ext_cau with version '1.0' schema issue6716;
begin;
set search_path=issue6716;
alter extension test_ext_cau update to '1.1';
end;

-- check search_path after transaction commit
show search_path;

reset search_path;
drop schema issue6716 cascade;
