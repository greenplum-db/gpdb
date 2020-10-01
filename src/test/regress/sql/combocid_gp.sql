--
-- Additional tests for combocids, for sharing the the array between QE
-- processes, and for growing the array.
--
-- These tests are mostly copied from the upstream 'combocid' test, but
-- the SELECT queries are replaced with a more complicated query that runs
-- on two slices.
--
CREATE TEMP TABLE combocidtest (foobar int, distkey int) distributed by (distkey);

BEGIN;

-- a few dummy ops to push up the CommandId counter
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;
INSERT INTO combocidtest SELECT 1 LIMIT 0;

INSERT INTO combocidtest VALUES (1);
INSERT INTO combocidtest VALUES (2);

SELECT ctid,cmin,* FROM combocidtest;

SAVEPOINT s1;

UPDATE combocidtest SET foobar = foobar + 10;

-- here we should see only updated tuples
SELECT * FROM combocidtest a, combocidtest b WHERE a.foobar < b.foobar;

ROLLBACK TO s1;

-- now we should see old tuples, but with combo CIDs starting at 0
SELECT * FROM combocidtest a, combocidtest b WHERE a.foobar < b.foobar;

COMMIT;

-- combo data is not there anymore, but should still see tuples
SELECT * FROM combocidtest a, combocidtest b WHERE a.foobar < b.foobar;

-- Test combo cids with portals
BEGIN;

INSERT INTO combocidtest VALUES (333);

DECLARE c CURSOR FOR SELECT * FROM combocidtest a, combocidtest b WHERE a.foobar < b.foobar;

DELETE FROM combocidtest;

FETCH ALL FROM c;

ROLLBACK;

SELECT * FROM combocidtest a, combocidtest b WHERE a.foobar < b.foobar;


--
-- Test growing the combocids array, including the shared combocids array.
--
CREATE TEMP TABLE manycombocids (i int, t text, distkey int) distributed by (distkey);
CREATE INDEX ON manycombocids (i);

BEGIN;

INSERT INTO manycombocids SELECT g, 'initially inserted',1 from generate_series(1, 10000) g;

-- update some of the rows. The combocids generated by this are included in
-- the initial snapshot that the first FETCH acquires.
DO $$
declare
  j int;
begin
  set enable_seqscan=off;
  for j in 1..10 loop
    UPDATE manycombocids set t = 'updated1' where i = j;
  end loop;
end;
$$;

-- Launch a query that will scan the table, using a cursor.
--
-- Requirements for this test query:
--
-- - it should run on at least two slices, so that the combocids array is
--   shared between the QE processes
--
-- - it should return the rows in a deterministic order, because we use MOVE to
--   skip rows. This is just to keep the expected output reasonably short.
--
-- - it mustn't materialize the whole result on the first FETCH. Otherwise, the
--   reader processes won't see the combocids that are created only after the
--   first FETCH.
--
set enable_indexonlyscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;
explain (costs off)  SELECT a.i, b.i, a.t FROM manycombocids a, manycombocids b WHERE a.i = b.i AND a.distkey=1;
DECLARE c CURSOR FOR SELECT a.i, b.i, a.t FROM manycombocids a, manycombocids b WHERE a.i = b.i AND a.distkey=1;

-- Start the cursor.
FETCH 1 FROM c;

-- Perform more updates.
DO $$
declare
  j int;
begin
  set enable_seqscan=off;
  for j in 1..1000 loop
    UPDATE manycombocids set t = 'updated2' where i = j * 10;
  end loop;
end;
$$;

-- Run the cursor to completion. This will encounter the combocids generated by the
-- previous updates, and should correctly see that the updates are not visible to
-- the cursor. (MOVE to keep the expected output at a reasonable size.)
MOVE 9900 FROM c;
FETCH ALL FROM c;

rollback;
