-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE ROLE ck_sync1_role1 with login;
CREATE ROLE ck_sync1_role2 with login;
CREATE ROLE ck_sync1_role3 with login;
CREATE ROLE ck_sync1_role4 with login;
CREATE ROLE ck_sync1_role5 with login;
CREATE ROLE ck_sync1_role6 with login;
CREATE ROLE ck_sync1_role7 with login;

DROP ROLE sync1_role2;
DROP ROLE ck_sync1_role1;
