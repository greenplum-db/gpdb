-- @Description Tests that DML operatins change the modification count.
-- 

SELECT state, tupcount, modcount FROM gp_aoseg_name('ao');
INSERT INTO ao VALUES (11, 11);
SELECT state, tupcount, modcount FROM gp_aoseg_name('ao');
DELETE FROM ao WHERE a = 11;
SELECT state, tupcount, modcount FROM gp_aoseg_name('ao');
UPDATE AO SET b = 0 WHERE a = 10;
SELECT state, tupcount, modcount FROM gp_aoseg_name('ao');
