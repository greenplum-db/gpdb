-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CREATE GROUP
--
CREATE GROUP sync2_group1 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
CREATE GROUP sync2_group2 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
--
--
--DROP GROUP
--
--
DROP GROUP sync1_group7;
DROP GROUP ck_sync1_group6;
DROP GROUP ct_group4;
DROP GROUP resync_group2;
DROP GROUP sync2_group1;
--
