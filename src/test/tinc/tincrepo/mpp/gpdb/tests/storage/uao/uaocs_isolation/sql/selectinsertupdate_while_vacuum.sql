-- @Description Ensures that an update during a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 2;
4: BEGIN;
4: SELECT COUNT(*) FROM ao;
4: INSERT INTO ao VALUES (1, 1);
4>: UPDATE ao SET b=1 WHERE a > 5;UPDATE ao SET b=1 WHERE a > 6;COMMIT;
2: VACUUM ao;
4<:
3: SELECT COUNT(*) FROM ao WHERE b = 1;
3: INSERT INTO ao VALUES (0);
