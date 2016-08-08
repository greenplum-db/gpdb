-- @Description Ensures that a vacuum with serializable works ok
-- 

DELETE FROM ao WHERE a < 20;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT COUNT(*) FROM ao;
SELECT segno, tupcount FROM gp_aoseg_name('ao');
VACUUM ao;
SELECT segno, tupcount FROM gp_aoseg_name('ao');
SELECT COUNT(*) FROM ao;
