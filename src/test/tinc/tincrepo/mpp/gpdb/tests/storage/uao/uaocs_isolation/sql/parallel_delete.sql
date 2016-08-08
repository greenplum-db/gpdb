-- @Description Tests that a delete operation in progress will block all other deletes
-- until the transaction is committed.
-- 

1: BEGIN;
2: BEGIN;
2: DELETE FROM ao WHERE a = 1;
2: SELECT * FROM locktest WHERE coalesce = 'ao';
1&: DELETE FROM ao WHERE a = 2;
2: COMMIT;
1<:
1: COMMIT;
3: BEGIN;
3: SELECT * FROM ao WHERE a < 5 ORDER BY a;
3: COMMIT;
2U: SELECT * FROM gp_aovisimap_name('ao');
