-- @Description Tests the update command with many blocks.
-- 

SELECT SUM(b) FROM foo WHERE a = 17;
UPDATE foo SET b = 2*b WHERE a = 17;
SELECT SUM(b) FROM foo WHERE a = 17;
BEGIN;
UPDATE foo SET b = 2*b WHERE a = 17;
COMMIT;
SELECT SUM(b) FROM foo WHERE a = 17;
BEGIN;
UPDATE foo SET b = 2*b WHERE a = 17;
SELECT SUM(b) FROM foo WHERE a = 17;
ROLLBACK;
SELECT SUM(b) FROM foo WHERE a = 17;
