-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned (e.g. because of a crash) in combination
-- with a partial index.
-- 

SET enable_seqscan=false;
DELETE FROM foo WHERE a < 16;
VACUUM foo;
SELECT * FROM foo WHERE b = 20;
SELECT * FROM foo WHERE b = 10;
INSERT INTO foo SELECT i as a, i as b, 'Good morning' as c FROM generate_series(101, 110) AS i;
SELECT * FROM foo WHERE b = 10;
SELECT * FROM foo WHERE b = 102;
