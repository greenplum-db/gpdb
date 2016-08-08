-- @Description Ensures that a select after a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 128;
1: BEGIN;
1: SELECT COUNT(*) FROM ao2;
2U: SELECT segno, tupcount FROM gp_aocsseg_name('ao');
2: VACUUM ao;
1: SELECT COUNT(*) FROM ao;
1: SELECT * FROM locktest WHERE coalesce = 'ao';
1: COMMIT;
1: SELECT COUNT(*) FROM ao;
3: INSERT INTO ao VALUES (0);
2U: SELECT segno, tupcount FROM gp_aocsseg_name('ao');
