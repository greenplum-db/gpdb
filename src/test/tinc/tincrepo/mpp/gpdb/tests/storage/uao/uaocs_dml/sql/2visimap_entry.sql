-- @Description Tests the delete feature with two visimap entries and delete jumping
-- between them.
-- 

SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
BEGIN;
DELETE FROM foo WHERE a = 99962;
DELETE FROM foo WHERE a = 5;
DELETE FROM foo WHERE a = 99964;
DELETE FROM foo WHERE a = 42;
DELETE FROM foo WHERE a IN (6, 97000, 99, 1700, 99980);
SELECT a, b FROM foo WHERE a >= 99960 AND a < 99970;
SELECT a, b FROM foo WHERE a < 10;
