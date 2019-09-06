-- 
-- ROLE
--

--
-- Test setting roles per-user and per user in a database.
--
-- This also covers an old bug with dispatching GUCs with units in ALTER ROLE
-- statement (MPP-15479).
--
DROP ROLE IF EXISTS role_setting_test_1;
DROP ROLE IF EXISTS role_setting_test_2;
DROP ROLE IF EXISTS role_setting_test_3;
DROP ROLE IF EXISTS role_setting_test_4;
DROP ROLE IF EXISTS role_setting_test_5;
DROP ROLE IF EXISTS role_setting_test_6;

CREATE ROLE role_setting_test_1 NOLOGIN;
CREATE ROLE role_setting_test_2 NOLOGIN;
CREATE ROLE role_setting_test_3 NOLOGIN;
CREATE ROLE role_setting_test_4 NOLOGIN;
CREATE ROLE role_setting_test_5 NOLOGIN;
CREATE ROLE role_setting_test_6 NOLOGIN;
CREATE SCHEMA common_schema;

/* Alter Role Set, with a GUC with units (statement_mem) and not (search_path) */
ALTER ROLE role_setting_test_1 SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_1 SET search_path = common_schema;
ALTER ROLE role_setting_test_2 IN DATABASE regression SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_2 IN DATABASE regression SET search_path = common_schema;

/* Alter Role Reset */
ALTER ROLE role_setting_test_3 SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_3 IN DATABASE regression SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_3 SET search_path = common_schema;
ALTER ROLE role_setting_test_3 IN DATABASE regression SET search_path = common_schema;

ALTER ROLE role_setting_test_3 RESET statement_mem;
ALTER ROLE role_setting_test_3 IN DATABASE regression RESET search_path;

/* Alter Role Reset All */
ALTER ROLE role_setting_test_5 SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_5 IN DATABASE regression SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_6 SET statement_mem TO '150MB';
ALTER ROLE role_setting_test_6 IN DATABASE regression SET statement_mem TO '150MB';

ALTER ROLE role_setting_test_5 RESET ALL;
ALTER ROLE role_setting_test_6 IN DATABASE regression RESET ALL;

\drds role_setting_test_*

-- Note: Don't drop the test roles, so that they get tested with pg_dump/restore, too,
-- if you dump the regression database.


-- SHA-256 testing
set password_hash_algorithm to "SHA-256";
create role sha256 password 'abc';
select rolname, rolpassword from pg_authid where rolname = 'sha256';
drop role sha256;

create role superuser;
create role u1;
set role superuser;

create table t1(a int, b int constraint c check (b>=100));
create view t1_view as select * from t1;
grant all privileges on t1, t1_view to u1;
set role superuser;
revoke all privileges on TABLE t1, t1_view FROM u1;
set role u1;
select * from t1_view order by 1;
reset role;
drop view t1_view;
drop table t1;
drop role u1;
drop role superuser;

-- Test creating user who has been renamed before.
CREATE USER jonathan11 WITH PASSWORD 'abc1';
CREATE USER jonathan12 WITH PASSWORD 'abc2';

ALTER USER jonathan11 RENAME TO jona11;
ALTER USER jonathan12 RENAME TO jona12;

DROP USER jona11;
DROP USER jona12;

CREATE USER jonathan12 WITH PASSWORD 'abc2';

ALTER USER jonathan11 RENAME TO jona11;
ALTER USER jonathan12 RENAME TO jona12;

CREATE GROUP marketing WITH USER jona11,jona12;

ALTER GROUP marketing RENAME TO market;

DROP GROUP market;
DROP USER jona11;
DROP USER jona12;

-- Test that a non-superuser cannot use ALTER USER RESET ALL to reset
-- superuser-only GUCs. (A bug that was fixed in PostgreSQL commit
-- e429448f33.)

-- First, drop old user, if it was left over from previous run of this test.
set client_min_messages='warning';
drop role if exists guctestrole;
reset client_min_messages;

-- Create a user with two per-user settings. One is superuser-only,
-- and another is not.
create user guctestrole;
alter user guctestrole set zero_damaged_pages=off; -- PGC_SUSET
alter user guctestrole set application_name='test'; -- PGC_USERSET
select rolconfig from pg_roles where rolname = 'guctestrole';

-- Switch to non-superuser role, and issue ALTER USER RESET ALL.
-- It should clear the 'application_name' setting, but not the
-- 'zero_damaged_pages' setting, because it's superuser-only.
set role guctestrole;
alter user guctestrole reset all;
select rolconfig from pg_roles where rolname = 'guctestrole';
reset role;

-- Test ALTER USER ALL
BEGIN;
ALTER USER ALL SET application_name TO 'alter_user_all_test';
ALTER USER ALL RESET ALL;
ROLLBACK;
