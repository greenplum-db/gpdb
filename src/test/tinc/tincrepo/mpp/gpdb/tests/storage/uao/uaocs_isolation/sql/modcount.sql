-- @Description Tests that DML operatins change the modification count.
-- 

SELECT DISTINCT state, tupcount, modcount FROM gp_aocsseg_name('ao');
INSERT INTO ao VALUES (11, 11);
SELECT DISTINCT state, tupcount, modcount FROM gp_aocsseg_name('ao');
DELETE FROM ao WHERE a = 11;
SELECT DISTINCT state, tupcount, modcount FROM gp_aocsseg_name('ao');
UPDATE AO SET b = 0 WHERE a = 10;
SELECT DISTINCT state, tupcount, modcount FROM gp_aocsseg_name('ao');
