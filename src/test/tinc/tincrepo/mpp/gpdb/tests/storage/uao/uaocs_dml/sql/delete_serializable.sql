-- @Description Tests that delete in a serializable transaction is not supported
-- 

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
DELETE FROM foo WHERE a < 4;
