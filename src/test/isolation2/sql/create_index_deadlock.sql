-- This test is used to check if there is a deadlock between coordinator and segments
-- when creating index.

1: CREATE TABLE test_create_index_deadlock_tbl (c1 int);

1: BEGIN;
1: CREATE INDEX test_create_index_deadlock_idx on test_create_index_deadlock_tbl (c1);
2&: VACUUM FULL pg_index;
1: SELECT * FROM test_create_index_deadlock_tbl;
1: COMMIT;
2<:

1: DROP TABLE test_create_index_deadlock_tbl;
