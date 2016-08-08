-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;

CREATE TABLE foo (a INT, b INT, c CHAR(128)) ;
CREATE INDEX foo_index ON foo(b);
CREATE INDEX foo_index2 ON foo(a);
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1,1000) AS i;
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(1001,2000) AS i;
INSERT INTO foo SELECT i as a, i as b, 'hello world' as c FROM generate_series(2001,3000) AS i;
