-- @Description Tests that delete in a serializable transaction is supported
-- 

BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
DELETE FROM foo WHERE a < 4;
