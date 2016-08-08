-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description HDFS permissions

/* Note that roles are defined at the system-level and are valid
 * for all databases in your Greenplum Database system. */
\echo '-- start_ignore'
revoke all on protocol gphdfs from _hadoop_perm_test_role;
DROP ROLE IF EXISTS _hadoop_perm_test_role;
revoke all on protocol gphdfs from _hadoop_perm_test_role2;
DROP ROLE IF EXISTS _hadoop_perm_test_role2;
\echo '-- end_ignore'

/* Now create a new role. Initially this role should NOT
 * be allowed to create an external hdfs table. */

CREATE ROLE _hadoop_perm_test_role
WITH CREATEEXTTABLE
LOGIN;

CREATE ROLE _hadoop_perm_test_role2
WITH CREATEEXTTABLE
LOGIN;

--ALTER ROLE _hadoop_perm_test_role
--WITH
--CREATEEXTTABLE(type='writable', protocol='gphdfs');
grant insert on protocol gphdfs to _hadoop_perm_test_role;

--ALTER ROLE _hadoop_perm_test_role
--WITH
--CREATEEXTTABLE(type='readable', protocol='gphdfs')
--CREATEEXTTABLE(type='writable');
grant all on protocol gphdfs to _hadoop_perm_test_role2;

