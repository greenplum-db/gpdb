-- For partitioned tables if a DML is run on the root table, we should
-- take ExclusiveLock on the root and the child partitions. If lock is
-- not taken on the leaf partition on the QD, there may be issues where
-- a concurrent DML/Vaccum etc on the leaf partition may execute first and can cause
-- issues. For example
-- 1. It can cause a deadlock between the 2 DML operations.
--      Consider the below example.
--      INSERT INTO part_tbl SELECT i, 1, i FROM generate_series(1,1000000)i;
--      Session 1: BEGIN; DELETE FROM part_tbl; ==> Let's say it holds Exclusive lock on root table only. (not on leaf)
--      Session 2: BEGIN; DELETE FROM part_tbl where a = 999999 or a = 1; ==> Delete will be dispatched to the segment as there is no lock on QD.
--      If Session 1, first deletes the tuple a = 1 on segment 0, Session 2 will wait for transaction lock as it attempting to delete the same tuple.
--      If Session 2, first deletes the tuple a = 999999 on segment 1, Session 1 will wait for transaction lock as it is attempting to delete the same tuple.
--      This will cause a deadlock.
--      Note: Same applies to UPDATE as well.
-- 2. If AO vacuum is run concurrently on the child partition, it may result in corrupting the segfile state.
--      Consider the below example:
--      Session 1: DELETE FROM part_tbl where a = 10000000; ==> Let's say it holds Exclusive lock on root table only. (not on leaf)
--      It will not take any lock on part_tbl_1_prt_1
--      Session 2: VACUUM part_tbl_1_prt;
--      VACUUM will be dispatched on the segment as there was no lock on QD. However, when in AO vacuum drop phase, it will try to acquire a lock on part_tbl_1_prt,
--      but that lock may already be taken by Session1, Vaccum will consider that this table has been dropped and will leave the segfile in an inconsitent
--      state, thus any consecutive operation touching the segfile will fail.
-- Refer to the commit message for detailed steps for the above examples for reference.
DROP TABLE IF EXISTS part_tbl;
CREATE TABLE part_tbl (a int, b int, c int) PARTITION BY RANGE (b) (start(1) end(2) every(1));

INSERT INTO part_tbl SELECT i, 1, i FROM generate_series(1,12)i;

1: BEGIN;
-- DELETE will acquire Exclusive lock on root and leaf partition on QD.
1: DELETE FROM part_tbl;

-- Delete must hold an exclusive lock on the leaf partition on QD.
SELECT GRANTED FROM pg_locks WHERE relation = 'part_tbl_1_prt_1'::regclass::oid AND mode='ExclusiveLock' AND gp_segment_id=-1 AND locktype='relation';

-- DELETE on the leaf partition must wait on the QD as Session 1 already holds ExclusiveLock.
2&: DELETE FROM part_tbl_1_prt_1 WHERE b = 10;
SELECT relation::regclass, mode, granted FROM pg_locks WHERE gp_segment_id=-1 AND granted='f';

1: COMMIT;
2<:

1: BEGIN;
-- UPDATE will acquire Exclusive lock on root and leaf partition on QD.
1: UPDATE part_tbl SET c = 1; 
SELECT GRANTED FROM pg_locks WHERE relation = 'part_tbl_1_prt_1'::regclass::oid AND mode='ExclusiveLock' AND gp_segment_id=-1 AND locktype='relation';

-- UPDATE on leaf must be blocked on QD as previous UPDATE acquires Exclusive lock on the root and the leaf partitions
2&: UPDATE part_tbl_1_prt_1 set c = 10;
SELECT relation::regclass, mode, granted FROM pg_locks WHERE gp_segment_id=-1 AND granted='f';

1: COMMIT;
2<:

1: BEGIN;
1: INSERT INTO part_tbl SELECT 1,1,1;
SELECT GRANTED FROM pg_locks WHERE relation = 'part_tbl_1_prt_1'::regclass::oid AND mode='RowExclusiveLock' AND gp_segment_id=-1 AND locktype='relation';
1: COMMIT;
DROP TABLE part_tbl;

-- Above scenarios do not affect non-partitioned tables, as there is no
-- root/child partitions so we should not take ExclusiveLock.
CREATE TABLE a_table_in_pgclass(a int);
1: BEGIN;
1: SET allow_system_table_mods=DML;
1: UPDATE pg_class SET relpages = 42 WHERE oid='a_table_in_pgclass'::regclass::oid;

-- If we had taken a lock on pg_class then subsequent CREATE TABLE statements
-- would block until lock is released. Check to ensure we do not block.
CREATE TABLE another_table_in_pgclass(a int);
1: COMMIT;
DROP TABLE a_table_in_pgclass;
DROP TABLE another_table_in_pgclass;
