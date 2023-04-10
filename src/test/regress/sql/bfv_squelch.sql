--
-- https://github.com/greenplum-db/gpdb/issues/11673
--

EXPLAIN ANALYSE SELECT nspname, nspacl, privilege_type, rolname FROM pg_namespace
JOIN lateral  
(SELECT * FROM aclexplode(nspacl) x 
JOIN pg_authid  ON x.grantee = pg_authid.oid) 
z ON true;

SET from_collapse_limit = 1;
SET join_collapse_limit = 1;

EXPLAIN ANALYSE SELECT nspname, nspacl, privilege_type, rolname FROM pg_namespace
JOIN lateral  
(SELECT * FROM aclexplode(nspacl) x 
JOIN pg_authid  ON x.grantee = pg_authid.oid) 
z ON true;