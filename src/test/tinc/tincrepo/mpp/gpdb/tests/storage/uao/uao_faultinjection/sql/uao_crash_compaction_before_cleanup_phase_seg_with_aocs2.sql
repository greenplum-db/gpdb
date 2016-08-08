-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
INSERT INTO foo VALUES(31, 1, 'c');
update foo set b = b +20 where a = 31;
SELECT a, b FROM foo order by a;
VACUUM foo;
INSERT INTO foo VALUES(32, 1, 'c');
SELECT a, b FROM foo order by a;
INSERT INTO fooaocs VALUES(31, 1, 'c');
SELECT a, b FROM fooaocs order by a;
