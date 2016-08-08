-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE ROLE ct_role1 with login;
CREATE ROLE ct_role2 with login;
CREATE ROLE ct_role3 with login;
CREATE ROLE ct_role4 with login;
CREATE ROLE ct_role5 with login;

DROP ROLE sync1_role4;
DROP ROLE ck_sync1_role3;
DROP ROLE ct_role1;
