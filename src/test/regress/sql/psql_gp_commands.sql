--
-- Test \dx and \dx+, to display extensions.
--
-- We just use gp_inject_fault as an example of an extension here. We don't
-- inject any faults.
-- start_ignore
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- end_ignore

\dx gp_inject*
\dx+ gp_inject*


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


-- pg_catalog.pg_roles.rolcreaterexthdfs
CREATE ROLE test_psql_du_e7 WITH SUPERUSER CREATEEXTTABLE (type = 'readable', protocol = 'gphdfs');
\du test_psql_du_e7
DROP ROLE test_psql_du_e7;


-- pg_catalog.pg_roles.rolcreatewexthdfs
CREATE ROLE test_psql_du_e8 WITH SUPERUSER CREATEEXTTABLE (type = 'writable', protocol = 'gphdfs');
\du test_psql_du_e8
DROP ROLE test_psql_du_e8;

-- Test replication and verbose. GPDB specific attributes are mixed with PG attributes.
-- Our role describe code is easy to be buggy when we merge with PG upstream code.
-- The tests here are used to double-confirm the correctness of our role describe code.
CREATE ROLE test_psql_du_e9 WITH SUPERUSER REPLICATION;
COMMENT ON ROLE test_psql_du_e9 IS 'test_role_description';
\du test_psql_du_e9
\du+ test_psql_du_e9
DROP ROLE test_psql_du_e9;


--
-- Test that \dE displays both external and foreign tables
--
CREATE FOREIGN DATA WRAPPER dummy_wrapper;
COMMENT ON FOREIGN DATA WRAPPER dummy_wrapper IS 'useless';
CREATE SERVER dummy_server FOREIGN DATA WRAPPER dummy_wrapper;
CREATE FOREIGN TABLE "dE_foreign_table" (c1 integer)
  SERVER dummy_server;

CREATE EXTERNAL TABLE "dE_external_table"  (c1 integer)
  LOCATION ('file://localhost/dummy') FORMAT 'text';

-- Change the owner, so that the expected output is not sensitive to current
-- username.
CREATE ROLE test_psql_de_role;
ALTER FOREIGN TABLE "dE_foreign_table" OWNER TO test_psql_de_role;
ALTER EXTERNAL TABLE "dE_external_table" OWNER TO test_psql_de_role;

\dE "dE"*

-- Clean up
DROP OWNED BY test_psql_de_role;
DROP ROLE test_psql_de_role;
