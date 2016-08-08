-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE ROLE sync2_role1 with login;
CREATE ROLE sync2_role2 with login;

DROP ROLE sync1_role7;
DROP ROLE ck_sync1_role6;
DROP ROLE ct_role4;
DROP ROLE resync_role2;
DROP ROLE sync2_role1;
