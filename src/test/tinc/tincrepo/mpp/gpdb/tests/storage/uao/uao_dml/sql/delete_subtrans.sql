-- @Description Tests that delete with sub transactions
-- 

BEGIN;
SAVEPOINT my_savepoint;
DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo;
ROLLBACK TO SAVEPOINT my_savepoint;
SELECT COUNT(*) FROM foo;
COMMIT;
BEGIN;
SELECT COUNT(*) FROM foo;
DELETE FROM foo WHERE a < 10;
SAVEPOINT my_savepoint2;
SELECT COUNT(*) FROM foo;
ROLLBACK TO SAVEPOINT my_savepoint2;
SELECT COUNT(*) FROM foo;
COMMIT;
BEGIN;
SELECT COUNT(*) FROM foo;
SAVEPOINT my_savepoint3;
DELETE FROM foo WHERE a < 20;
SAVEPOINT my_savepoint4;
SELECT COUNT(*) FROM foo;
ROLLBACK TO SAVEPOINT my_savepoint3;
SELECT COUNT(*) FROM foo;
COMMIT;
