--
-- Test \dx and \dx+, to display extensions.
--
-- We just use gp_inject_fault as an example of an extension here. We don't
-- inject any faults.
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

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
