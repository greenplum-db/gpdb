-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;

CREATE TABLE foo (a INT, b INT, c CHAR(128)) ;
CREATE INDEX foo_index ON foo(b);
INSERT INTO foo VALUES (0, 64000, 'c');
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 10) AS i;
INSERT INTO foo VALUES (0, 90002, 'c');
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 60000) AS i;
INSERT INTO foo VALUES (0, 64001, 'c');
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(70001, 120000) AS i;
ANALYZE foo;
REINDEX INDEX foo_index;

