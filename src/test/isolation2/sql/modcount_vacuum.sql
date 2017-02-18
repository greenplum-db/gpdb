-- @Description Tests that vacuum is not changing the modification count.
-- 
DROP TABLE IF EXISTS ao;
CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1,10) AS i;

DELETE FROM ao WHERE a < 5;
SELECT state, tupcount, modcount FROM gp_toolkit.__gp_aoseg_name('ao');
VACUUM ao;
SELECT state, tupcount, modcount FROM gp_toolkit.__gp_aoseg_name('ao');
