-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS fooaocs;

CREATE TABLE foo (a INT, b INT, c CHAR(128)) WITH (appendonly=true);
CREATE INDEX foo_index ON foo(b);
INSERT INTO foo SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;
DELETE FROM foo WHERE a < 4;

CREATE TABLE fooaocs (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column);
CREATE INDEX fooaocs_index ON fooaocs(b);
INSERT INTO fooaocs SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 10) AS i;
