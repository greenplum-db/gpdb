-- @Description Tests that vacuum is not changing the modification count.
-- 

DELETE FROM ao WHERE a < 5;
SELECT state, tupcount, modcount FROM gp_aoseg_name('ao');
VACUUM ao;
SELECT state, tupcount, modcount FROM gp_aoseg_name('ao');
