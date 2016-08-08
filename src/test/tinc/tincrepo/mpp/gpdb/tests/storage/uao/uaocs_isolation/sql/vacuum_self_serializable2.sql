-- @Description Ensures that a vacuum with serializable works ok
-- 

DELETE FROM ao WHERE a < 20;
1: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: SELECT COUNT(*) FROM ao;
1: BEGIN;
1: SELECT COUNT(*) FROM ao2;
2: SELECT DISTINCT segno, tupcount FROM gp_aocsseg_name('ao') ORDER BY segno;
2: VACUUM ao;
2: SELECT DISTINCT segno, tupcount FROM gp_aocsseg_name('ao') ORDER BY segno;
1: SELECT COUNT(*) FROM ao;
1: COMMIT;
