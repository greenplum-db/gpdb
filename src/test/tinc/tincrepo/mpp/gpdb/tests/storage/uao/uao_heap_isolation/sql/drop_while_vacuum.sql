-- @Description Ensures that a drop table while a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 12000;
1: SELECT COUNT(*) FROM ao;
1>: DROP TABLE ao;
2: VACUUM ao;
1<:
1: SELECT COUNT(*) FROM ao;
