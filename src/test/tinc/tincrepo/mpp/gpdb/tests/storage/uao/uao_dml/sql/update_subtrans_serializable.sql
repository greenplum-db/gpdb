-- @Description Tests that update in a serializable transaction is not supported
-- even in a subtransaction.
-- 

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SAVEPOINT my_savepoint;
UPDATE foo SET b = 1 WHERE a < 4;
