-- @Description Ensures that an alter table while a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 12000;
1: SELECT COUNT(*) FROM ao;
1>: ALTER TABLE ao ADD COLUMN d bigint DEFAULT 10;
2: VACUUM ao;
1<:
1: SELECT * FROM ao WHERE a < 12010;
