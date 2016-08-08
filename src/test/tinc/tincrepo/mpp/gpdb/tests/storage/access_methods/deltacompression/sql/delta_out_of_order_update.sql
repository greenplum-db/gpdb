-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- taken from uao test_suite

-- start_ignore
DROP TABLE IF EXISTS delta_one;
DROP TABLE IF EXISTS delta_two;
-- end_ignore

-- This contrived choice of distribution key is to ensure that
-- subsequent UPDATE operations happen on a single GPDB segment.
-- Otherwise, we may not exercise the case of out-of-order updates.
CREATE TABLE delta_one (a INT, b INT, c CHAR(100)) WITH (appendonly=true, orientation=column, compresstype=rle_type)
DISTRIBUTED BY(b);
INSERT INTO delta_one SELECT i as a, 1 as b, '' as c FROM generate_series(1, 100000) AS i;
INSERT INTO delta_one SELECT i as a, 2 as b, '' as c FROM generate_series(1, 100000) AS i;
CREATE TABLE delta_two (a INT, b INT, c CHAR(100)) DISTRIBUTED BY(b);
-- Insert unique values for delta_two.a so that we don't get "multiple
-- updates to a row by the same query is not allowed" error later when
-- we join delta_one and delta_two on a in update statements.  This particular
-- error is covered by the test case "doubleupdate_command.sql".


INSERT INTO delta_two SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 2 as b, '' as c  FROM generate_series(1, 100000) AS i;
ANALYZE delta_one;
ANALYZE delta_two;

set enable_nestloop=false;
UPDATE delta_one SET a = 0 FROM delta_two WHERE delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 10 OR delta_two.a = 40000 OR delta_two.a = 20000);

-- Ensure that tuples to be updated are all from the same GPDB segment.
SELECT distinct(a) FROM delta_one WHERE gp_segment_id = 0 AND
delta_one.a IN (12, 80001, 1001) ORDER BY a;

UPDATE delta_one SET a = 1 FROM delta_two WHERE
delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 12 OR delta_two.a = 80001 OR delta_two.a = 1001);

