-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS ao;

CREATE TABLE ao (a INT, b INT) WITH  (appendonly=true, orientation=column);
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1, 100000) AS i;

