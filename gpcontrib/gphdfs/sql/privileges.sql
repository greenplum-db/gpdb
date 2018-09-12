--
-- test CREATE EXTERNAL TABLE privileges
--
CREATE ROLE exttab1_su SUPERUSER; -- SU with no privs in pg_auth
CREATE ROLE exttab1_u4 CREATEEXTTABLE(protocol='gphdfs', type='readable'); 
CREATE ROLE exttab1_u5 CREATEEXTTABLE(protocol='gphdfs', type='writable'); 
CREATE ROLE exttab1_u6 NOCREATEEXTTABLE(protocol='gphdfs', type='readable') NOCREATEEXTTABLE(protocol='gphdfs', type='writable');
CREATE ROLE exttab1_u7 CREATEEXTTABLE(protocol='gphdfs') NOCREATEEXTTABLE(protocol='gphdfs', type='readable'); -- fail due to conflict 
CREATE ROLE exttab1_u7 CREATEEXTTABLE(protocol='gphdfs', type='writable') NOCREATEEXTTABLE(protocol='gphdfs', type='writable'); -- fail due to conflict 

SET SESSION AUTHORIZATION exttab1_u4;
create external table auth_ext_test5(a int) location ('gphdfs://host:8000/file') format 'text';
create writable external table auth_ext_test6(a int) location ('gphdfs://host:8000/file') format 'text'; -- fail
RESET SESSION AUTHORIZATION;
SET SESSION AUTHORIZATION exttab1_u5;
create writable external table auth_ext_test7(a int) location ('gphdfs://host:8000/file') format 'text';
create external table auth_ext_test8(a int) location ('gphdfs://host:8000/file') format 'text';           -- fail
RESET SESSION AUTHORIZATION;
SET SESSION AUTHORIZATION exttab1_u6;
create writable external table auth_ext_test9(a int) location ('gphdfs://host:8000/file') format 'text';  -- fail
create external table auth_ext_test9(a int) location ('gphdfs://host:8000/file') format 'text';           -- fail
RESET SESSION AUTHORIZATION;


drop external table auth_ext_test5;
drop external table auth_ext_test7;

DROP ROLE exttab1_su;
DROP ROLE exttab1_u4;
DROP ROLE exttab1_u5;
DROP ROLE exttab1_u6;
