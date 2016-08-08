-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
INSERT INTO fooaocs VALUES(25, 6, 'c');
update fooaocs set b = b+10 where a=25;
SELECT a, b FROM fooaocs order by a;
VACUUM fooaocs;
INSERT INTO fooaocs VALUES(21, 1, 'c');
SELECT a, b FROM fooaocs order by a;
INSERT INTO foo VALUES(26, 1, 'c');
update foo set b = b+10 where a=26;
SELECT a, b FROM foo order by a;
