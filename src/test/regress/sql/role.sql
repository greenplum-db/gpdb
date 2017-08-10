-- 
-- ROLE
--

-- MPP-15479: ALTER ROLE SET statement
DROP ROLE IF EXISTS role_112911;
CREATE ROLE role_112911 WITH LOGIN;
CREATE SCHEMA common_schema;

/* Alter Role Set statement_mem */
ALTER ROLE role_112911 SET statement_mem TO '150MB';
SELECT gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM pg_authid WHERE rolname = 'role_112911'
 UNION ALL
SELECT DISTINCT 0 as gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM gp_dist_random('pg_authid') WHERE rolname = 'role_112911';

/* Alter Role Set search_path */
ALTER ROLE role_112911 SET search_path = common_schema;
SELECT gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM pg_authid WHERE rolname = 'role_112911'
 UNION ALL
SELECT DISTINCT 0 as gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM gp_dist_random('pg_authid') WHERE rolname = 'role_112911';

/* Alter Role Reset statement_mem */
ALTER ROLE role_112911 RESET statement_mem;
SELECT gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM pg_authid WHERE rolname = 'role_112911'
 UNION ALL
SELECT DISTINCT 0 as gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM gp_dist_random('pg_authid') WHERE rolname = 'role_112911';

/* Alter Role Set statement_mem */
ALTER ROLE role_112911 SET statement_mem = 100000;
SELECT gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM pg_authid WHERE rolname = 'role_112911'
 UNION ALL
SELECT DISTINCT 0 as gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM gp_dist_random('pg_authid') WHERE rolname = 'role_112911';

/* Alter Role Reset All */
ALTER ROLE role_112911 RESET ALL;
SELECT gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM pg_authid WHERE rolname = 'role_112911'
 UNION ALL
SELECT DISTINCT 0 as gp_segment_id, rolname, array_to_string(rolconfig,',') as rolconfig
  FROM gp_dist_random('pg_authid') WHERE rolname = 'role_112911';

-- Set search_path to a non-existent schema. This used to (incorrectly)
-- print a NOTICE from each segment. (MPP-3068)
alter role role_112911 set search_path to blahblah1;

DROP ROLE role_112911;
DROP SCHEMA common_schema;

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
