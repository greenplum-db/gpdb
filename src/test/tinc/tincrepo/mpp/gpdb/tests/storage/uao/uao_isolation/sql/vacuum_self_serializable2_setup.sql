-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS ao;
DROP TABLE IF EXISTS ao2;

CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
CREATE TABLE ao2 (a INT) WITH (appendonly=true);
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1, 100) AS i;
