-- @Description Ensures that a serializable select before during a vacuum operation avoids the compaction.
-- 

DELETE FROM ao WHERE a < 128;
1: BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: SELECT COUNT(*) FROM ao;
2U: SELECT segno, tupcount FROM gp_aoseg_name('ao');
2: VACUUM ao;
1: SELECT COUNT(*) FROM ao;
1: COMMIT;
3: INSERT INTO ao VALUES (0);
2U: SELECT segno, tupcount FROM gp_aoseg_name('ao');
