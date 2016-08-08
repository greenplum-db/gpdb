-- @Description Ensures that an update before a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 12;
1: BEGIN;
1: SELECT COUNT(*) FROM ao;
1>: UPDATE ao SET b=1 WHERE a > 0;COMMIT;
2: VACUUM ao;
1<:
1: SELECT COUNT(*) FROM ao;
3: INSERT INTO ao VALUES (0);
