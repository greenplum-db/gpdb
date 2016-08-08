-- @Description Tests that update is supported in a subtransaction.
-- 

BEGIN;
SAVEPOINT my_savepoint;
SELECT COUNT(*) FROM foo WHERE b = 1;
UPDATE foo SET b = 1 WHERE a < 4;
SAVEPOINT my_savepoint2;
ROLLBACK To my_savepoint;
SELECT COUNT(*) FROM foo WHERE b = 1;
UPDATE foo SET b = 1 WHERE a < 4;
COMMIT;

BEGIN;
SAVEPOINT my_savepoint;
SELECT COUNT(*) FROM foo WHERE b = 1;
UPDATE foo SET b = 1 WHERE a < 8;
SAVEPOINT my_savepoint2;
UPDATE foo SET b = 2 WHERE a < 12;
SELECT COUNT(*) FROM foo WHERE b = 1;
ROLLBACK To my_savepoint2;
SELECT COUNT(*) FROM foo WHERE b = 1;
ROLLBACK To my_savepoint;
SELECT COUNT(*) FROM foo WHERE b = 1;
COMMIT;
