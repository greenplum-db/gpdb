-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;

CREATE TABLE foo (a INT, b INT) WITH (appendonly=true);
INSERT INTO foo VALUES (0, 0);
INSERT INTO foo VALUES (1, 1);
