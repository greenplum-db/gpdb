-- @Description Checks that deleting works with many AO blocks.
-- 

SELECT COUNT(*) FROM foo WHERE b = 17;
DELETE FROM foo WHERE b = 17;
SELECT COUNT(*) FROM foo WHERE b = 17;
SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE b = 16 OR b = 10000 OR b = 15;
SELECT COUNT(*) FROM foo;
