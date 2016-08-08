-- @Description Ensures that a select before a vacuum operation is ok
-- 

DELETE FROM ao WHERE a < 128;
1: BEGIN;
1: SELECT COUNT(*) FROM ao;
1: SELECT * FROM locktest WHERE coalesce = 'ao';
2&: VACUUM ao;
1: COMMIT;
2<:
1: SELECT COUNT(*) FROM ao;
3: INSERT INTO ao VALUES (0);
