-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists foo;
drop table if exists bar;
CREATE TABLE foo (a INT, b int, c int, d int) with (appendonly = true);
CREATE TABLE bar (a INT, b int, c int, d int) with (appendonly = true);
INSERT INTO foo SELECT i as a,i,i,i  FROM generate_series(1, 100000) AS i;
INSERT INTO bar SELECT i as a,i,i,i  FROM generate_series(1, 100000) AS i;
