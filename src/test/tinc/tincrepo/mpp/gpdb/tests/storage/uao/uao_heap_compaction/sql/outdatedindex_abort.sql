-- @Description Tests the behavior when the index of an ao table
-- has not been cleaned (e.g. because of a crash) in combination
-- with aborted inserts.
-- 

SET enable_seqscan=false;
DELETE FROM foo WHERE a < 16;
VACUUM foo;
SELECT * FROM foo WHERE b = 20;
SELECT * FROM foo WHERE b = 10;
INSERT INTO foo SELECT i as a, i as b, 'Good morning' as c FROM generate_series(1, 4) AS i;
BEGIN;
INSERT INTO foo SELECT i as a, i as b, 'Good morning' as c FROM generate_series(5, 8) AS i;
INSERT INTO foo SELECT i as a, i as b, 'Good morning' as c FROM generate_series(9, 12) AS i;
ROLLBACK;
SELECT * FROM foo WHERE b < 16;
