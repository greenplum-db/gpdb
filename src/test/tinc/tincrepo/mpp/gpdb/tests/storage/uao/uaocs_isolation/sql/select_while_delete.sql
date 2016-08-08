-- @Description Ensures that a select during a delete operation is ok
-- 

2: BEGIN;
2: SELECT * FROM ao WHERE a < 5 ORDER BY a;
2: DELETE FROM ao WHERE a < 5;
1: SELECT * FROM ao WHERE a >= 5 AND a < 10 ORDER BY a;
3: SELECT * FROM ao WHERE a < 5 ORDER BY a;
2: COMMIT;
2: SELECT * FROM ao WHERE a < 5 ORDER BY a;
4: SELECT * FROM ao WHERE a < 10 ORDER BY a;
2U: SELECT * FROM gp_aovisimap_name('ao');
4: INSERT INTO ao VALUES (0);
