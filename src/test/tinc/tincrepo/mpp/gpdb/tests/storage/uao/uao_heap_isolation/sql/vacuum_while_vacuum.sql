-- @Description Ensures that an vacuum while a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 1200;
1: SELECT COUNT(*) FROM ao;
1>: VACUUM ao;
2: VACUUM ao;
1<:
1: SELECT COUNT(*) FROM ao;
3: INSERT INTO ao VALUES (0);
