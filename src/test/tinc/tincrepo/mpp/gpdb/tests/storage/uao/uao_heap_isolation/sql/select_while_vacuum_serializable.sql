-- @Description Ensures that a select from a serializalbe transaction is ok after vacuum
-- 

DELETE FROM ao WHERE a < 128;
1: BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
2: VACUUM ao;
1: SELECT COUNT(*) FROM ao;
3: INSERT INTO ao VALUES (0);
