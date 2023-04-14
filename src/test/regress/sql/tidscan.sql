-- tests for tidscans

CREATE TABLE tidscan(id integer);

-- GPDB: we need some preparation, to make the output the same as in upstream.
-- Firstly, force all the rows to the same segment, so that selecting by ctid
-- produces the same result as in upstream.
ALTER TABLE tidscan ADD COLUMN distkey int;
ALTER TABLE tidscan SET DISTRIBUTED BY (distkey);

-- Secondly, Coerce the planner to produce same plans as in upstream.
set enable_seqscan=off;
set enable_mergejoin=on;
set enable_nestloop=on;

-- Finally, silence NOTICEs that GPDB normally emits if you use ctid in a
-- query:
-- NOTICE:  SELECT uses system-defined column "tidscan.ctid" without the necessary companion column "tidscan.gp_segment_id"
-- HINT:  To uniquely identify a row within a distributed table, use the "gp_segment_id" column together with the "ctid" column.
set client_min_messages='warning';

-- only insert a few rows, we don't want to spill onto a second table page
INSERT INTO tidscan (id) VALUES (1), (2), (3);
ANALYZE tidscan;

-- The 'distkey' column has served its purpose, by ensuring that all the rows
-- end up on the same segment. Now drop it, so that it doesn't affect the
-- output of the "select *" queries that follow.
ALTER TABLE tidscan DROP COLUMN distkey;

-- show ctids
SELECT ctid, * FROM tidscan;

-- ctid equality - implemented as tidscan
EXPLAIN (COSTS OFF)
SELECT ctid, * FROM tidscan WHERE ctid = '(0,1)';
SELECT ctid, * FROM tidscan WHERE ctid = '(0,1)';

EXPLAIN (COSTS OFF)
SELECT ctid, * FROM tidscan WHERE '(0,1)' = ctid;
SELECT ctid, * FROM tidscan WHERE '(0,1)' = ctid;

-- OR'd clauses
EXPLAIN (COSTS OFF)
SELECT ctid, * FROM tidscan WHERE ctid = '(0,2)' OR '(0,1)' = ctid;
SELECT ctid, * FROM tidscan WHERE ctid = '(0,2)' OR '(0,1)' = ctid;

-- ctid = ScalarArrayOp - implemented as tidscan
EXPLAIN (COSTS OFF)
SELECT ctid, * FROM tidscan WHERE ctid = ANY(ARRAY['(0,1)', '(0,2)']::tid[]);
SELECT ctid, * FROM tidscan WHERE ctid = ANY(ARRAY['(0,1)', '(0,2)']::tid[]);

-- ctid != ScalarArrayOp - can't be implemented as tidscan
EXPLAIN (COSTS OFF)
SELECT ctid, * FROM tidscan WHERE ctid != ANY(ARRAY['(0,1)', '(0,2)']::tid[]);
SELECT ctid, * FROM tidscan WHERE ctid != ANY(ARRAY['(0,1)', '(0,2)']::tid[]);

-- tid equality extracted from sub-AND clauses
EXPLAIN (COSTS OFF)
SELECT ctid, * FROM tidscan
WHERE (id = 3 AND ctid IN ('(0,2)', '(0,3)')) OR (ctid = '(0,1)' AND id = 1);
SELECT ctid, * FROM tidscan
WHERE (id = 3 AND ctid IN ('(0,2)', '(0,3)')) OR (ctid = '(0,1)' AND id = 1);

-- nestloop-with-inner-tidscan joins on tid
SET enable_hashjoin TO off;  -- otherwise hash join might win
EXPLAIN (COSTS OFF)
SELECT t1.ctid, t1.*, t2.ctid, t2.*
FROM tidscan t1 JOIN tidscan t2 ON t1.ctid = t2.ctid WHERE t1.id = 1;
SELECT t1.ctid, t1.*, t2.ctid, t2.*
FROM tidscan t1 JOIN tidscan t2 ON t1.ctid = t2.ctid WHERE t1.id = 1;
EXPLAIN (COSTS OFF)
SELECT t1.ctid, t1.*, t2.ctid, t2.*
FROM tidscan t1 LEFT JOIN tidscan t2 ON t1.ctid = t2.ctid WHERE t1.id = 1;
SELECT t1.ctid, t1.*, t2.ctid, t2.*
FROM tidscan t1 LEFT JOIN tidscan t2 ON t1.ctid = t2.ctid WHERE t1.id = 1;
RESET enable_hashjoin;

-- exercise backward scan and rewind
BEGIN;
DECLARE c CURSOR FOR
SELECT ctid, * FROM tidscan WHERE ctid = ANY(ARRAY['(0,1)', '(0,2)']::tid[]);
FETCH ALL FROM c;
FETCH BACKWARD 1 FROM c;
FETCH FIRST FROM c;
ROLLBACK;

-- tidscan via CURRENT OF
BEGIN;
DECLARE c CURSOR FOR SELECT ctid, * FROM tidscan;
FETCH NEXT FROM c; -- skip one row
FETCH NEXT FROM c;
-- perform update
EXPLAIN (ANALYZE, COSTS OFF, SUMMARY OFF, TIMING OFF)
UPDATE tidscan SET id = -id WHERE CURRENT OF c RETURNING *;
FETCH NEXT FROM c;
-- perform update
EXPLAIN (ANALYZE, COSTS OFF, SUMMARY OFF, TIMING OFF)
UPDATE tidscan SET id = -id WHERE CURRENT OF c RETURNING *;
SELECT * FROM tidscan;
-- position cursor past any rows
FETCH NEXT FROM c;
-- should error out
EXPLAIN (ANALYZE, COSTS OFF, SUMMARY OFF, TIMING OFF)
UPDATE tidscan SET id = -id WHERE CURRENT OF c RETURNING *;
ROLLBACK;

-- bulk joins on CTID
-- (these plans don't use TID scans, but this still seems like an
-- appropriate place for these tests)
reset enable_seqscan;
EXPLAIN (COSTS OFF)
SELECT count(*) FROM tenk1 t1 JOIN tenk1 t2 ON t1.ctid = t2.ctid and t1.gp_segment_id = t2.gp_segment_id;
SELECT count(*) FROM tenk1 t1 JOIN tenk1 t2 ON t1.ctid = t2.ctid and t1.gp_segment_id = t2.gp_segment_id;
SET enable_hashjoin TO off;
EXPLAIN (COSTS OFF)
SELECT count(*) FROM tenk1 t1 JOIN tenk1 t2 ON t1.ctid = t2.ctid and t1.gp_segment_id = t2.gp_segment_id;
SELECT count(*) FROM tenk1 t1 JOIN tenk1 t2 ON t1.ctid = t2.ctid and t1.gp_segment_id = t2.gp_segment_id;
RESET enable_hashjoin;

-- check predicate lock on CTID
-- GPDB_12_MERGE_FEATURE_NOT_SUPPORTED: Greenplum does not support serializable transactions,
-- ingore the below test case.
-- start_ignore
BEGIN ISOLATION LEVEL SERIALIZABLE;
SELECT * FROM tidscan WHERE ctid = '(0,1)';
-- locktype should be 'tuple'
SELECT locktype, mode FROM pg_locks WHERE pid = pg_backend_pid() AND mode = 'SIReadLock';
ROLLBACK;
-- end_ignore

DROP TABLE tidscan;
