-- @Description Tests the error message of the cluster command
-- 
SELECT * FROM ao;
CLUSTER ao_index ON ao;
SELECT * FROM ao;
