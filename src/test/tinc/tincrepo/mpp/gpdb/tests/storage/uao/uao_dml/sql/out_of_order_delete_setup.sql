-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;

-- This contrived choice of distribution key is to ensure that
-- subsequent DELETE operations happen on a single GPDB segment.
-- Otherwise, we may not exercise the case of out-of-order delete.
CREATE TABLE foo (a INT, b INT, c CHAR(100)) WITH (appendonly=true)
DISTRIBUTED BY (b);
INSERT INTO foo SELECT i as a, 1 as b, '' as c FROM generate_series(1, 100000) AS i;
INSERT INTO foo SELECT i as a, 2 as b, '' as c FROM generate_series(1, 100000) AS i;
CREATE TABLE bar (a INT, b INT, c CHAR(100)) DISTRIBUTED BY (b);
-- Populate bar such that at least one segment contains multiple
-- occurences of the same value for a.
INSERT INTO bar SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO bar SELECT i as a, 2 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO bar SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO bar SELECT i as a, 2 as b, '' as c  FROM generate_series(1, 100000) AS i;
ANALYZE foo;
ANALYZE bar;

