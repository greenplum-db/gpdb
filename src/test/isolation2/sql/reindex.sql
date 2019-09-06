-- Dropping table while reindex database should not fail reindex
CREATE DATABASE reindexdb1 TEMPLATE template1;
-- halt reindex after scanning the pg_class and getting the relids
SELECT gp_inject_fault_infinite('reindex_db', 'suspend', 1);
1:@db_name reindexdb1: CREATE TABLE heap1(a INT, b INT);
1&:REINDEX DATABASE reindexdb1;
SELECT gp_wait_until_triggered_fault('reindex_db', 1, 1);
2:@db_name reindexdb1:DROP TABLE heap1;
SELECT gp_inject_fault('reindex_db', 'reset', 1);
-- reindex should complete fine
1<:
1q:
2q:

-- Adding index after scanning indexes for relation to reindex should
-- not fail reindex
BEGIN;
CREATE TABLE reindex_index1(a int, b int);
CREATE INDEX reindex_index1_idx1 on reindex_index1 (b);
insert into reindex_index1 select i,i+1 from generate_series(1, 10)i;
COMMIT;
SELECT gp_inject_fault_infinite('reindex_relation', 'suspend', 1);
3&: REINDEX TABLE reindex_index1;
SELECT gp_wait_until_triggered_fault('reindex_relation', 1, 1);
-- create one more index
CREATE INDEX reindex_index1_idx2 on reindex_index1 (a);
SELECT gp_inject_fault('reindex_relation', 'reset', 1);
3<:

DROP DATABASE reindexdb1;
