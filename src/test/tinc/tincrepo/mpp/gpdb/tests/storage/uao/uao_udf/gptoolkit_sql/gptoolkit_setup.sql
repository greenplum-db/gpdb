-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a INT, b INT) WITH (appendonly=true);
INSERT INTO foo SELECT i as a, i as b FROM generate_series(1,20) AS i;
UPDATE foo SET b = 0 WHERE a = 1;
DELETE FROM foo WHERE a = 2;

DROP TABLE IF EXISTS foocs;
CREATE TABLE foocs (a INT, b INT) WITH (appendonly=true, orientation=column);
INSERT INTO foocs SELECT i as a, i as b FROM generate_series(1,20) AS i;
UPDATE foocs SET b = 0 WHERE a = 1;
DELETE FROM foocs WHERE a = 2;
