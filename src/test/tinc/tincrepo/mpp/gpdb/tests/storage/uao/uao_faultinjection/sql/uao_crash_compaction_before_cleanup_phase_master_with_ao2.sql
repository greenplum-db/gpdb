-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
INSERT INTO fooaocs VALUES(5, 6, 'c');
update fooaocs set b = b+10 where a=5;
SELECT a, b FROM fooaocs;
VACUUM fooaocs;
INSERT INTO fooaocs VALUES(1, 1, 'c');
SELECT a, b FROM fooaocs;
INSERT INTO foo VALUES(6, 1, 'c');
update foo set b = b+10 where a=6;
SELECT a, b FROM foo;
