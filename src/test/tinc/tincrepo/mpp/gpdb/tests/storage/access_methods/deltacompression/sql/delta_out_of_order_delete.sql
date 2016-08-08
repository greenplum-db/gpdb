-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- taken from uao test_suite

-- start_ignore
DROP TABLE IF EXISTS delta_one;
DROP TABLE IF EXISTS delta_two;
-- end_ignore

-- This contrived choice of distribution key is to ensure that
-- subsequent DELETE operations happen on a single GPDB segment.
-- Otherwise, we may not exercise the case of out-of-order delete.
CREATE TABLE delta_one (a INT, b INT, c CHAR(100)) WITH (appendonly=true, orientation=column, compresstype=rle_type)
DISTRIBUTED BY (b);

\d+ delta_one
INSERT INTO delta_one SELECT i as a, 1 as b, '' as c FROM generate_series(1, 100000) AS i;
INSERT INTO delta_one SELECT i as a, 2 as b, '' as c FROM generate_series(1, 100000) AS i;
CREATE TABLE delta_two (a INT, b INT, c CHAR(100)) DISTRIBUTED BY (b);
-- Populate delta_two such that at least one segment contains multiple
-- occurences of the same value for a.
INSERT INTO delta_two SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 2 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 1 as b, '' as c  FROM generate_series(1, 100000) AS i;
INSERT INTO delta_two SELECT i as a, 2 as b, '' as c  FROM generate_series(1, 100000) AS i;
ANALYZE delta_one;
ANALYZE delta_two;


DELETE FROM delta_one USING delta_two WHERE delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 10 OR delta_two.a = 40000 OR delta_two.a = 20000);

-- Ensure that tuples to be deleted are from the same GPDB segment.
-- This query should return the same output irrespective of GPDB
-- configuration (1 segdb, 2 or more segdbs).

SELECT distinct(a) FROM delta_one
WHERE gp_segment_id = 0 AND delta_one.a IN (12, 80001, 1001)
ORDER BY a;

DELETE FROM delta_one USING delta_two WHERE delta_one.b = delta_two.b AND delta_one.a = delta_two.a AND
(delta_two.a = 12 OR delta_two.a = 80001 OR delta_two.a = 1001);
