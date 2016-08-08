-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS checkpoint_tbl, another_table;

CREATE TABLE checkpoint_tbl(a int, b int);
INSERT INTO checkpoint_tbl SELECT i, i FROM generate_series(1, 1000)i;
DROP TABLE checkpoint_tbl;

CHECKPOINT;

CREATE TABLE another_table(c text);

