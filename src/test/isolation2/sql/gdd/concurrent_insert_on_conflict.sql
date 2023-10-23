-- 
-- Test case for concurrent insert on conflict and drop a table
-- 
CREATE TABLE t_concurrent_insert(a int primary key, b int);

1: BEGIN;
1: DROP TABLE t_concurrent_insert;
2&: INSERT INTO t_concurrent_insert VALUES(1, 1) ON CONFLICT(a) DO UPDATE SET b = excluded.b;
1: COMMIT;

-- insert failed, rather than segment fault
2<:
1q:
2q: