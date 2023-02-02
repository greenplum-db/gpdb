-- Tests to ensure that unique indexes work as expected w/ ao_row tables.

-- We use a replicated table to test each table for ease in testing edge cases
-- where conflicts arise at block directory boundaries. We can treat the table
-- as if it were being populated in utility mode on a single segment, allowing
-- us to predict block directory entries without having to worry about the
-- table's distribution.

-- Case 1: Conflict with committed transaction----------------------------------
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
-- should conflict
INSERT INTO unique_index_ao_row VALUES (1);
INSERT INTO unique_index_ao_row VALUES (329729);
-- should not conflict
INSERT INTO unique_index_ao_row VALUES (329730);
DROP TABLE unique_index_ao_row;

-- Case 2: Conflict within the same transaction---------------------------------
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
BEGIN;
INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
-- should conflict
INSERT INTO unique_index_ao_row VALUES (1);
END;
DROP TABLE unique_index_ao_row;

CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
BEGIN;
INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
-- should conflict
INSERT INTO unique_index_ao_row VALUES (329729);
END;
DROP TABLE unique_index_ao_row;

CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
BEGIN;
INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
-- should not conflict
INSERT INTO unique_index_ao_row VALUES (329730);
END;
DROP TABLE unique_index_ao_row;

-- Case 3: Conflict with aborted transaction is not a conflict------------------
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
BEGIN;
INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
ABORT;
-- should not conflict
INSERT INTO unique_index_ao_row VALUES (1);
INSERT INTO unique_index_ao_row VALUES (329729);
INSERT INTO unique_index_ao_row VALUES (329730);
DROP TABLE unique_index_ao_row;

-- Case 4: Conflict with to-be-committed transaction----------------------------
--
-- 1. Uncommitted tx 1 has inserted non-conflicting key = 0.
-- 2. Uncommitted tx 2 has inserted (161 * 2048 + 1 = 329729 rows), which spans
--    2 block directory rows (1st row: [1,329728] ; 2nd row: [329729,329729])
-- 3. Tx 3 tries to insert conflicting key = 2, which maps to the second rownum
--    covered by the 1st block directory row of seg 1, and blocks on tx 2.
-- 4. Tx 4 tries to insert conflicting key = 329728, which maps to the last
--    rownum covered by the 1st block directory row of seg 1, and blocks on tx 2.
-- 5. Tx 5 tries to insert conflicting key = 329729, which maps to the first
--    rownum covered by the 2nd block directory row of seg 1, and blocks on tx 2.
-- 6. Tx 6 tries to insert non-conflicting key = 329730 and is immediately
--    successful.
-- 7. Tx 2 commits
-- 8. Txs 3,4,5 report unique constraint violation
-- 9. Tx 1 commits
--
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
1: BEGIN;
1: INSERT INTO unique_index_ao_row VALUES (0);
2: BEGIN;
2: INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
3&: INSERT INTO unique_index_ao_row VALUES (1);
4&: INSERT INTO unique_index_ao_row VALUES (329728);
5&: INSERT INTO unique_index_ao_row VALUES (329729);
-- should succeed immediately
6: INSERT INTO unique_index_ao_row VALUES (329730);
2: COMMIT;
3<:
4<:
5<:
1: COMMIT;
DROP TABLE unique_index_ao_row;

-- Case 5: Conflict with to-be-aborted transaction------------------------------
--
-- 1. Uncommitted tx 1 has inserted non-conflicting key = 0.
-- 2. Uncommitted tx 2 has inserted (161 * 2048 + 1 = 329729 rows), which spans
--    2 block directory rows (1st row: [1,329728] ; 2nd row: [329729,329729])
-- 3. Tx 3 tries to insert conflicting key = 2, which maps to the second rownum
--    covered by the 1st block directory row of seg 1, and blocks on tx 2.
-- 4. Tx 4 tries to insert conflicting key = 329728, which maps to the last
--    rownum covered by the 1st block directory row of seg 1, and blocks on tx 2.
-- 5. Tx 5 tries to insert conflicting key = 329729, which maps to the first
--    rownum covered by the 2nd block directory row of seg 1, and blocks on tx 2.
-- 6. Tx 6 tries to insert non-conflicting key = 329730 and is immediately
--    successful.
-- 8. Tx 2 aborts
-- 9. Txs 3,4,5 report unique constraint violation
-- 10. Tx 1 commits
--
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
1: BEGIN;
1: INSERT INTO unique_index_ao_row VALUES (0);
2: BEGIN;
2: INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 329729);
3&: INSERT INTO unique_index_ao_row VALUES (1);
4&: INSERT INTO unique_index_ao_row VALUES (329728);
5&: INSERT INTO unique_index_ao_row VALUES (329729);
-- should succeed immediately
6: INSERT INTO unique_index_ao_row VALUES (329730);
2: ABORT;
3<:
4<:
5<:
1: COMMIT;
DROP TABLE unique_index_ao_row;

-- Case 6: Conflict with aborted rows following some committed rows ------------
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
-- 1. Tx 1 commits rows 1-100.
-- 2. Tx 2 inserts rows 101-200 and then aborts.
-- 3. Tx 3 tries to insert row in range [101,200] and is immediately successful.
-- 4. Tx 4 tries to insert conflicting row in range [1,100] and raises unique
--    constraint violation.
-- 5. Tx 5 tries to insert row in range [201, ) and is immediately successful.
1: INSERT INTO unique_index_ao_row SELECT generate_series(1, 100);
2: BEGIN;
2: INSERT INTO unique_index_ao_row SELECT generate_series(101, 200);
2: ABORT;
3: INSERT INTO unique_index_ao_row VALUES(102);
4: INSERT INTO unique_index_ao_row VALUES(2);
5: INSERT INTO unique_index_ao_row VALUES(202);
DROP TABLE unique_index_ao_row;

--------------------------------------------------------------------------------
----------------- More concurrent tests with fault injection ------------------
--------------------------------------------------------------------------------

-- Case 7: Conflict with to-be-committed transaction while only 1 placeholder
-- row exists in the block directory--------------------------------------------
--
-- This case highlights the importance of the placeholder row, inserted at the
-- beginning of an INSERT command.
--
-- 1. Uncommitted Tx 1 has inserted 3 out of its 10 rows and is suspended.
-- 2. Tx 2 inserts a conflicting row and blocks on Tx 1.
-- 3. Tx 3 inserts a non-conflicting row within the range [4,10] and is
--    immediately successful. (Index entries have been written only for [1,3] so
--    far, so conflicts shouldn't arise)
-- 4. Tx 4 inserts a non-conflicting row in range [11, ..) and should be
--    immediately successful.
-- 5. Now Tx 1 resumes and tries to insert a row in range [4,10] and reports a
--    unique constraint violation with Tx 3.
-- 6. Tx 2 succeeds as Tx 1 aborted.

CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;
SELECT gp_inject_fault('appendonly_insert', 'suspend', '', '', 'unique_index_ao_row', 4, 4, 0, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
1&: INSERT INTO unique_index_ao_row SELECT * FROM generate_series(1, 10);
-- Wait until 3 rows have been successfully inserted into the index and Tx 1
-- is just beginning to insert the 4th row.
SELECT gp_wait_until_triggered_fault('appendonly_insert', 4, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
2&: INSERT INTO unique_index_ao_row VALUES(2);
4: INSERT INTO unique_index_ao_row VALUES(11);
3: INSERT INTO unique_index_ao_row VALUES(4);
SELECT gp_inject_fault('appendonly_insert', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
1<:
2<:
DROP TABLE unique_index_ao_row;

-- Case 8: Conflict with to-be-committed transaction - generalization of case 7
-- where there are multiple minipages (and block directory rows) in play from
-- the same insert.
--
-- This justifies why 1 placeholder row is enough and we don't need to flush a
-- placeholder row every time we insert a block directory row (i.e. start a new
-- in-memory minipage) throughout the course of a single insert.
--
-- 1. Uncommitted Tx 1 has inserted (2048 * (161 * 2 + 1) + 3) = 661507 rows
--    and is suspended, enough rows to fill 2 entire minipages (covers
--    range [1,329728] and [329729,659456]) before suspension.
-- 2. Txs 2,3,4 inserts conflicting rows that map to the 1st minipage and block.
-- 3. Txs 5,6,7 inserts conflicting rows that map to the 2nd minipage and block.
-- 4. Tx 8 inserts a conflicting row that maps to the 3rd minipage, which is
--    currently only in-memory and it conflicts on the placeholder row and
--    blocks (showcases why 1 placeholder row is enough)
-- 5. Tx 9 inserts a non-conflicting row for which there is no index entry and
--    and is immediately successful (661510).
-- 6. Now Tx 1 resumes and tries to insert 661510 and reports a unique
--    constraint violation with Tx 9.
-- 7. All blocked Txs succeed.

CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;

SELECT gp_inject_fault('insert_new_entry_curr_minipage_full', 'suspend', '', '', '', 2, 2, 0, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
1&: INSERT INTO unique_index_ao_row SELECT generate_series(1, 661510);

-- Wait until we have inserted (2048 * (161 * 2 + 1) + 3) = 661507 rows and we
-- are about to insert the 661508th row.
SELECT gp_wait_until_triggered_fault('insert_new_entry_curr_minipage_full', 2, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
SELECT gp_inject_fault('appendonly_insert', 'suspend', '', '', 'unique_index_ao_row', 4, 4, 0, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
SELECT gp_inject_fault('insert_new_entry_curr_minipage_full', 'reset', dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;
SELECT gp_wait_until_triggered_fault('appendonly_insert', 4, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;

-- maps to 1st minipage
2&: INSERT INTO unique_index_ao_row VALUES(1);
3&: INSERT INTO unique_index_ao_row VALUES(300000);
4&: INSERT INTO unique_index_ao_row VALUES(329728);
-- maps to 2nd minipage
5&: INSERT INTO unique_index_ao_row VALUES(329729);
6&: INSERT INTO unique_index_ao_row VALUES(598000);
7&: INSERT INTO unique_index_ao_row VALUES(659456);
-- maps to 3rd minipage
8&: INSERT INTO unique_index_ao_row VALUES(661507);
-- no index entry exists for it, so should not conflict.
9: INSERT INTO unique_index_ao_row VALUES(661510);

SELECT gp_inject_fault('appendonly_insert', 'reset', dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content <> -1;

1<:
2<:
3<:
4<:
5<:
6<:
7<:
8<:

DROP TABLE unique_index_ao_row;

--------------------------------------------------------------------------------
--------------------------- Smoke tests for COPY -------------------------------
--------------------------------------------------------------------------------

CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;

1: BEGIN;
1: COPY unique_index_ao_row FROM PROGRAM 'seq 1 10';
-- concurrent tx inserting conflicting row should block.
2&: COPY unique_index_ao_row FROM PROGRAM 'seq 1 1';
-- concurrent tx inserting non-conflicting rows should be successful.
3: COPY unique_index_ao_row FROM PROGRAM 'seq 11 20';
-- inserting a conflicting row in the same transaction should ERROR out.
1: COPY unique_index_ao_row FROM PROGRAM 'seq 1 1';
-- now that tx 1 was aborted, tx 2 is successful.
2<:
1: END;

DROP TABLE unique_index_ao_row;

--------------------------------------------------------------------------------
-------------------- Smoke tests for subtransactions ---------------------------
--------------------------------------------------------------------------------
CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;

1: BEGIN;
1: SAVEPOINT a;
1: INSERT INTO unique_index_ao_row VALUES(1);

-- concurrent tx inserting conflicting row should block.
2: BEGIN;
2&: INSERT INTO unique_index_ao_row VALUES(1);
-- concurrent tx inserting non-conflicting row should be successful.
3: INSERT INTO unique_index_ao_row VALUES(2);

-- conflict should be detected within the same subtx.
1: INSERT INTO unique_index_ao_row VALUES(1);
-- the concurrent tx should now succeed.
2<:
2: ABORT;

-- after rolling back to the savepoint, we should be able to re-insert the key
1: ROLLBACK TO SAVEPOINT a;
1: INSERT INTO unique_index_ao_row VALUES(1);
1: COMMIT;

SELECT * FROM unique_index_ao_row;

DROP TABLE unique_index_ao_row;

--------------------------------------------------------------------------------
-------------------- Smoke tests for repeatable read ---------------------------
--------------------------------------------------------------------------------

-- Test that shows that unique index checks transcend transaction isolation
-- boundaries.

CREATE TABLE unique_index_ao_row (a INT unique) USING ao_row
    DISTRIBUTED REPLICATED;

-- Begin two txs with tx level snapshot taken early.
1: BEGIN ISOLATION LEVEL REPEATABLE READ;
1: SELECT * FROM unique_index_ao_row;
2: BEGIN ISOLATION LEVEL REPEATABLE READ;
2: SELECT * FROM unique_index_ao_row;

-- Now begin a concurrent transaction which inserts a key.
3: BEGIN;
3: INSERT INTO unique_index_ao_row VALUES(1);

-- And another transaction inserts a key and commits.
INSERT INTO unique_index_ao_row VALUES(2);

-- Tx should block on insert of conflicting key, even though it can't "see" the
-- conflicting key due to its isolation level.
1: SELECT * FROM unique_index_ao_row;
1&: INSERT INTO unique_index_ao_row VALUES(1);

3: ABORT;
1<:
1: ABORT;

-- Tx should raise a conflict, even though it can't "see" the conflicting key
-- due to its isolation level.
2: SELECT * FROM unique_index_ao_row;
2: INSERT INTO unique_index_ao_row VALUES(2);
2: ABORT;

DROP TABLE unique_index_ao_row;
