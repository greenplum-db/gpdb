-- @Description Tests that vacuum is not changing the modification count.
-- 

DELETE FROM ao WHERE a < 5;
SELECT DISTINCT state, tupcount, modcount FROM gp_aocsseg_name('ao');
VACUUM ao;
SELECT DISTINCT state, tupcount, modcount FROM gp_aocsseg_name('ao');
