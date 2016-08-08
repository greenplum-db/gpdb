-- @Description The locks held after different operations
-- 

1: BEGIN;
1: INSERT INTO ao VALUES (200, 200);
SELECT * FROM locktest;
2U: SELECT * FROM locktest;
1: COMMIT;
1: BEGIN;
1: DELETE FROM ao WHERE a = 1;
SELECT * FROM locktest;
2U: SELECT * FROM locktest;
1: COMMIT;
1: BEGIN;
1: UPDATE ao SET b = -1 WHERE a = 2;
SELECT * FROM locktest;
2U: SELECT * FROM locktest;
1: COMMIT;
