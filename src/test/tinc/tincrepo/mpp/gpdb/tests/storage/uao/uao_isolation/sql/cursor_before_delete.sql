-- @Description Tests the visibility when a cursor has been created before the delete.
-- 

1: BEGIN;
1: DECLARE cur CURSOR FOR SELECT a FROM ao ORDER BY a;
1: FETCH NEXT IN cur;
1: FETCH NEXT IN cur;
2: BEGIN;
2: DELETE FROM ao WHERE a < 5;
2: COMMIT;
1: FETCH NEXT IN cur;
1: FETCH NEXT IN cur;
1: FETCH NEXT IN cur;
1: CLOSE cur;
1: COMMIT;
3: BEGIN;
3: DECLARE cur CURSOR FOR SELECT a FROM ao ORDER BY a;
3: FETCH NEXT IN cur;
3: CLOSE cur;
3: COMMIT;
