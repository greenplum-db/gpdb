-- @Description Tests that aborting a delete works as expected.
-- 

SELECT COUNT(*) FROM foo;
BEGIN;
DELETE FROM foo WHERE a = 10;
SELECT COUNT(*) FROM foo;
ROLLBACK;
SELECT COUNT(*) FROM foo;
