-- @Description Ensures that a vacuum with serializable works ok
-- 

DELETE FROM ao WHERE a < 20;
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT COUNT(*) FROM ao;
SELECT DISTINCT segno, tupcount FROM gp_aocsseg_name('ao') ORDER BY segno;
VACUUM ao;
SELECT DISTINCT segno, tupcount FROM gp_aocsseg_name('ao') ORDER BY segno;
SELECT COUNT(*) FROM ao;
