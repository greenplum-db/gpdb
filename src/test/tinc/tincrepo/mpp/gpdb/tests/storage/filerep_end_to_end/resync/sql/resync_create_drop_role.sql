-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE ROLE resync_role1 with login;
CREATE ROLE resync_role2 with login;
CREATE ROLE resync_role3 with login;

DROP ROLE sync1_role6;
DROP ROLE ck_sync1_role5;
DROP ROLE ct_role3;
DROP ROLE resync_role1;
