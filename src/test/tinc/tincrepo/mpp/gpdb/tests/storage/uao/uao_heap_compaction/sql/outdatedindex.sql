-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned (e.g. because of a crash).
-- 

SET enable_seqscan=false;
DELETE FROM foo WHERE a < 16;
VACUUM foo;
SELECT * FROM foo WHERE b = 20;
SELECT * FROM foo WHERE b = 10;
INSERT INTO foo SELECT i as a, i as b, 'Good morning' as c FROM generate_series(1, 10) AS i;
SELECT * FROM foo WHERE b = 10;
