-- @Description Tests basic update commands on ao tables.
-- 

UPDATE foo SET b=42 WHERE a < 10;
SELECT COUNT(b) FROM foo WHERE b = 42;
SELECT COUNT(b) FROM foo WHERE b < 10;
UPDATE foo SET b=1001 WHERE b < 4;
DELETE FROM foo WHERE a < 4;
UPDATE foo SET b=1002 WHERE a < 5;
