-- @Description Tests that deleting tuples twice is handled as expected.
-- 

SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
