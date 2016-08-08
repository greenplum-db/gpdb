-- @Description Tests that a update operation in progress will block all other updates
-- until the transaction is committed.
-- 

1: BEGIN;
2: BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;
1: UPDATE ao SET b = 42 WHERE b = 1;
2&: UPDATE ao SET b = -1 WHERE b = 1;
1: COMMIT;
2<:
2: COMMIT;
SELECT * FROM ao WHERE b < 2;
