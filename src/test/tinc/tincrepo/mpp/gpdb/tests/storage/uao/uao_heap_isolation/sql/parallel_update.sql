-- @Description Tests that a update operation in progress will block all other updates
-- until the transaction is committed.
-- 

1: BEGIN;
2: BEGIN;
2: UPDATE ao SET b = 42 WHERE a = 1;
2: SELECT * FROM locktest WHERE coalesce = 'ao';
1&: UPDATE ao SET b = 42 WHERE a = 2;
2: COMMIT;
1<:
1: COMMIT;
3: SELECT * FROM ao WHERE a < 5 ORDER BY a;
