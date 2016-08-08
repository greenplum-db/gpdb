-- @Description Tests that update in a serializable transaction is not supported
-- 

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
UPDATE foo SET b = 1 WHERE a < 4;
