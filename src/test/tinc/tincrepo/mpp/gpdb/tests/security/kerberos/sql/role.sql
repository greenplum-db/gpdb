-- @product_version gpdb: [4.3.2-4.3.98]
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

DROP ROLE role_112911;
DROP SCHEMA common_schema;

-- SHA-256 testing
set password_hash_algorithm to "SHA-256";
create role sha256 password 'abc';
-- MPP-15865
-- OpenSSL SHA2 returning a different SHA2 to RSA BSAFE!
--select rolname, rolpassword from pg_authid where rolname = 'sha256';
drop role sha256;
