-- Run some basic tests on the gp_toolkit functions that deal with AO segfiles.
-- The exact output varies depending on cluster configuration and concurrent
-- activity in the system, so for most functions, we only do count(*). That
-- at least verifies that they don't crash.

-- Create test tables
--
-- We use DISTRIBUTED BY (c), and have the same c value in all the rows, to
-- put all the rows on the same segment. That way, the test output doesn't
-- depend on the number of segments.

DROP TABLE IF EXISTS toolkit_ao_test;
CREATE TABLE toolkit_ao_test (a INT, b INT, c INT)
  WITH (appendonly=true) DISTRIBUTED BY (c);
CREATE INDEX ON toolkit_ao_test(a);
INSERT INTO toolkit_ao_test SELECT i as a, i as b, 1 FROM generate_series(1,20) AS i;
UPDATE toolkit_ao_test SET b = 0 WHERE a = 1;
DELETE FROM toolkit_ao_test WHERE a = 2;

DROP TABLE IF EXISTS toolkit_aocs_test;
CREATE TABLE toolkit_aocs_test (a INT, b INT, C INT)
  WITH (appendonly=true, orientation=column) DISTRIBUTED BY (c);
CREATE INDEX ON toolkit_aocs_test(a);
INSERT INTO toolkit_aocs_test SELECT i as a, i as b FROM generate_series(1,20) AS i;
UPDATE toolkit_aocs_test SET b = 0 WHERE a = 1;
DELETE FROM toolkit_aocs_test WHERE a = 2;

-- Investigate them with the gp_toolkit functions
SELECT count(*) FROM gp_toolkit.__gp_aoseg_history('toolkit_ao_test');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('toolkit_aocs_test');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg_history('toolkit_aocs_test');
SELECT count(*) FROM gp_toolkit.__gp_aoseg_history('toolkit_ao_test');
SELECT * FROM gp_toolkit.__gp_aovisimap('toolkit_ao_test');
SELECT count(*) FROM gp_toolkit.__gp_aovisimap_hidden_info('toolkit_ao_test');
SELECT * FROM gp_toolkit.__gp_aovisimap_entry('toolkit_ao_test');
SELECT count(*) FROM gp_toolkit.__gp_aoseg('toolkit_ao_test');
SELECT * FROM gp_toolkit.__gp_aoblkdir('toolkit_ao_test');
SELECT * FROM gp_toolkit.__gp_aoblkdir('toolkit_aocs_test');

-- The same, but on the segments.
SELECT (t).* FROM (
  SELECT gp_toolkit.__gp_aovisimap('toolkit_ao_test') AS t FROM gp_dist_random('gp_id')
) AS x;
SELECT (t).segno, (t).first_row_num, (t).hidden_tupcount >= 1 as hidden_tupcount_nonzero, (t).bitmap like '01%' as bitmap_starts_with_01 FROM (
  SELECT gp_toolkit.__gp_aovisimap_entry('toolkit_ao_test') AS t FROM gp_dist_random('gp_id')
) AS x;
SELECT (t).* FROM (
  SELECT gp_toolkit.__gp_aoblkdir('toolkit_ao_test') AS t FROM gp_dist_random('gp_id')
) AS x;
SELECT (t).* FROM (
  SELECT gp_toolkit.__gp_aoblkdir('toolkit_aocs_test') AS t FROM gp_dist_random('gp_id')
) AS x;
