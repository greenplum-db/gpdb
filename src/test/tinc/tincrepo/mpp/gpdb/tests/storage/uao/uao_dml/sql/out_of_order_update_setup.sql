-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;

-- This contrived choice of distribution key is to ensure that
-- subsequent UPDATE operations happen on a single GPDB segment.
-- Otherwise, we may not exercise the case of out-of-order updates.
CREATE TABLE foo (a INT, b INT, c CHAR(100)) WITH (appendonly=true)
DISTRIBUTED BY(b);
INSERT INTO foo SELECT i as a, 1 as b, '' as c FROM generate_series(1, 100000) AS i;
INSERT INTO foo SELECT i as a, 2 as b, '' as c FROM generate_series(1, 100000) AS i;
CREATE TABLE bar (a INT, b INT, c CHAR(100)) DISTRIBUTED BY(b);
-- Insert unique values for bar.a so that we don't get "multiple
-- updates to a row by the same query is not allowed" error later when
-- we join foo and bar on a in update statements.  This particular
-- error is covered by the test case "doubleupdate_command.sql".
INSERT INTO bar SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO bar SELECT i as a, 2 as b, '' as c  FROM generate_series(1, 100000) AS i;
ANALYZE foo;
ANALYZE bar;

