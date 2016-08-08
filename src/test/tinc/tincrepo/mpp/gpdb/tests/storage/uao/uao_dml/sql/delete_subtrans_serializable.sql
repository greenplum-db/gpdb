-- @Description Tests that delete in a serializable transaction is not supported
-- even in a subtransaction.
-- 

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SAVEPOINT my_savepoint;
DELETE FROM foo WHERE a < 4;
