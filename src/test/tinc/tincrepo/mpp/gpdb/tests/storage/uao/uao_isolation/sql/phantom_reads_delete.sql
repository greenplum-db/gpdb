-- @Description Tests the basic phantom read behavior of GPDB w.r.t to delete using
-- the default isolation level.
-- 

1: BEGIN;
1: SELECT * FROM ao WHERE b BETWEEN 20 AND 30 ORDER BY a;
2: BEGIN;
2: DELETE FROM AO where b = 25;
2: COMMIT;
1: SELECT * FROM ao WHERE b BETWEEN 20 AND 30 ORDER BY a;
1: COMMIT;
