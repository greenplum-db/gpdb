-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
INSERT INTO foo VALUES(1, 1, 'c');
SELECT a, b FROM foo order by a;
VACUUM foo;
INSERT INTO foo VALUES(1, 1, 'c');
SELECT a, b FROM foo order by a;
