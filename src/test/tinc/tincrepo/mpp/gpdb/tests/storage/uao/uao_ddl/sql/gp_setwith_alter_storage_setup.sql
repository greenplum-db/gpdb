-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS atsdb;
create table atsdb (i int, j int, t text, n numeric) distributed by (j);
INSERT INTO atsdb VALUES (0, 0, 'hello world', 0);
INSERT INTO atsdb VALUES (1, 1, 'hello world', 1);
INSERT INTO atsdb VALUES (2, 2, 'hello world', 2);
