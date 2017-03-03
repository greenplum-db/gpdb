-- @Description Ensures that a delete before a vacuum operation is ok
-- @author Dirk Meister

DELETE FROM ao WHERE a < 12;
1: BEGIN;
1: SELECT COUNT(*) FROM ao;
1>: DELETE FROM ao WHERE a < 90;COMMIT;
2: VACUUM ao;
1<:
1: SELECT COUNT(*) FROM ao;
3: INSERT INTO ao VALUES (0);
