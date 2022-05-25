--
-- Test \dx and \dx+, to display extensions.
--
-- We just use plpgsql as an example of an extension here.
\dx plpgsql
\dx+ plpgsql

--
-- Test extended \du flags
--
-- https://github.com/greenplum-db/gpdb/issues/1028
--
-- Problem: the cluster can be initialized with any Unix user
--          therefore create specific test roles here, and only
--          test the \du output for this role, also drop them afterwards

CREATE ROLE test_psql_du_1 WITH SUPERUSER;
\du test_psql_du_1
DROP ROLE test_psql_du_1;

CREATE ROLE test_psql_du_2 WITH SUPERUSER CREATEDB CREATEROLE CREATEEXTTABLE LOGIN CONNECTION LIMIT 5;
\du test_psql_du_2
DROP ROLE test_psql_du_2;

-- pg_catalog.pg_roles.rolcreaterextgpfd
CREATE ROLE test_psql_du_e1 WITH SUPERUSER CREATEEXTTABLE (type = 'readable', protocol = 'gpfdist');
\du test_psql_du_e1
DROP ROLE test_psql_du_e1;

CREATE ROLE test_psql_du_e2 WITH SUPERUSER CREATEEXTTABLE (type = 'readable', protocol = 'gpfdists');
\du test_psql_du_e2
DROP ROLE test_psql_du_e2;


-- pg_catalog.pg_roles.rolcreatewextgpfd
CREATE ROLE test_psql_du_e3 WITH SUPERUSER CREATEEXTTABLE (type = 'writable', protocol = 'gpfdist');
\du test_psql_du_e3
DROP ROLE test_psql_du_e3;

CREATE ROLE test_psql_du_e4 WITH SUPERUSER CREATEEXTTABLE (type = 'writable', protocol = 'gpfdists');
\du test_psql_du_e4
DROP ROLE test_psql_du_e4;


-- pg_catalog.pg_roles.rolcreaterexthttp
CREATE ROLE test_psql_du_e5 WITH SUPERUSER CREATEEXTTABLE (type = 'readable', protocol = 'http');
\du test_psql_du_e5
DROP ROLE test_psql_du_e5;

-- does not exist
CREATE ROLE test_psql_du_e6 WITH SUPERUSER CREATEEXTTABLE (type = 'writable', protocol = 'http');
\du test_psql_du_e6
DROP ROLE test_psql_du_e6;


-- Test replication and verbose. GPDB specific attributes are mixed with PG attributes.
-- Our role describe code is easy to be buggy when we merge with PG upstream code.
-- The tests here are used to double-confirm the correctness of our role describe code.
CREATE ROLE test_psql_du_e9 WITH SUPERUSER REPLICATION;
COMMENT ON ROLE test_psql_du_e9 IS 'test_role_description';
\du test_psql_du_e9
\du+ test_psql_du_e9
DROP ROLE test_psql_du_e9;


--
-- Test \d commands.
--
-- Create a test schema, with different kinds of relations. To make the
-- expected output insensitive to the current username, change the owner.
CREATE ROLE test_psql_de_role;

CREATE FOREIGN DATA WRAPPER dummy_wrapper;
COMMENT ON FOREIGN DATA WRAPPER dummy_wrapper IS 'useless';
CREATE SERVER dummy_server FOREIGN DATA WRAPPER dummy_wrapper;

CREATE SCHEMA test_psql_schema;
GRANT CREATE, USAGE ON SCHEMA test_psql_schema TO test_psql_de_role;
SET search_path = 'test_psql_schema';
SET ROLE test_psql_de_role;

CREATE TABLE d_heap (i int4) with (appendonly = false);
CREATE TABLE d_ao (i int4) with (appendonly = true, orientation = row);
CREATE TABLE d_aocs (i int4) with (appendonly = true, orientation = column);
CREATE VIEW d_view as SELECT 123;
CREATE INDEX d_index on d_heap(i);

-- Only superuser can create external or foreign tables.
RESET ROLE;

CREATE FOREIGN TABLE "dE_foreign_table" (c1 integer)
  SERVER dummy_server;
ALTER FOREIGN TABLE "dE_foreign_table" OWNER TO test_psql_de_role;

CREATE EXTERNAL TABLE "dE_external_table"  (c1 integer)
  LOCATION ('file://localhost/dummy') FORMAT 'text';
ALTER EXTERNAL TABLE "dE_external_table" OWNER TO test_psql_de_role;

-- There's a GPDB-specific Storage column.
\d
\d+

-- The Storage column is not interesting for indexes, so it's omitted with
-- \di
\di
\di+

-- But if tables are shown, too, then it's interesting again.
\dti

-- \dE should display both external and foreign tables
\dE "dE"*
\dE

-- \dd should list objects having comments
\dd
create rule dd_notify as on update to d_heap do also notify d_heap;
comment on rule dd_notify on d_heap is 'this is a rule';
alter table d_heap add constraint dd_ichk check (i>20);
comment on constraint dd_ichk on d_heap is 'this is a constraint';
create operator family dd_opfamily using btree;
comment on operator family dd_opfamily using btree is 'this is an operator family';
\dd

-- \df+ should list all exec locations
CREATE FUNCTION foofunc_exec_on_any() RETURNS int AS 'SELECT 1' LANGUAGE SQL EXECUTE ON ANY;
ALTER FUNCTION foofunc_exec_on_any() OWNER TO test_psql_de_role;
CREATE FUNCTION foofunc_exec_on_coordinator() RETURNS setof int AS 'SELECT 1' LANGUAGE SQL EXECUTE ON COORDINATOR;
ALTER FUNCTION foofunc_exec_on_coordinator() OWNER TO test_psql_de_role;
CREATE FUNCTION foofunc_exec_on_all_segments() RETURNS setof int AS 'SELECT 1' LANGUAGE SQL EXECUTE ON ALL SEGMENTS;
ALTER FUNCTION foofunc_exec_on_all_segments() OWNER TO test_psql_de_role;
CREATE FUNCTION foofunc_exec_on_initplan() RETURNS setof int AS 'SELECT 1' LANGUAGE SQL EXECUTE ON INITPLAN;
ALTER FUNCTION foofunc_exec_on_initplan() OWNER TO test_psql_de_role;
\df+ foofunc_exec_on_*

-- Clean up
DROP OWNED BY test_psql_de_role;
DROP ROLE test_psql_de_role;
