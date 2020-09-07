--
-- Tests for some likely failure cases with combo cmin/cmax mechanism
--
-- GPDB: add a 'distkey' column to force all the rows to the same segment, so that
-- the rows have the same TIDs as in upstream.
CREATE TEMP TABLE combocidtest (foobar int, distkey int) distributed by (distkey);

-- GPDB: Disable autostats, because the queries it increments the command
-- counter, making the cmin/cmax values stored in the table different from
-- upstream.
set gp_autostats_mode=none;

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
SELECT ctid,cmin,* FROM combocidtest;

ROLLBACK TO s1;

-- now we should see old tuples, but with combo CIDs starting at 0
SELECT ctid,cmin,* FROM combocidtest;

COMMIT;

-- combo data is not there anymore, but should still see tuples
SELECT ctid,cmin,* FROM combocidtest;

-- Test combo cids with portals
BEGIN;

INSERT INTO combocidtest VALUES (333);

-- In GPDB, the cursor starts executing in the segments as soon as the
-- DECLARE is dispatched. Usually, the cursor will fetch the first row
-- from the table before the DELETE runs, so that it will see cmin==0
-- on the first row. But sometimes, the DELETE will update the row first,
-- so that the cursor will see cmin==1. Both are OK, but because it's
-- non-deterministic, don't display the cmin value.
DECLARE c CURSOR FOR SELECT ctid,* FROM combocidtest;

DELETE FROM combocidtest;

FETCH ALL FROM c;

ROLLBACK;

SELECT ctid,cmin,* FROM combocidtest;

-- check behavior with locked tuples
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

INSERT INTO combocidtest VALUES (444);

SELECT ctid,cmin,* FROM combocidtest;

SAVEPOINT s1;

-- this doesn't affect cmin
SELECT ctid,cmin,* FROM combocidtest FOR UPDATE;
SELECT ctid,cmin,* FROM combocidtest;

-- but this does
UPDATE combocidtest SET foobar = foobar + 10;

SELECT ctid,cmin,* FROM combocidtest;

ROLLBACK TO s1;

SELECT ctid,cmin,* FROM combocidtest;

COMMIT;

SELECT ctid,cmin,* FROM combocidtest;

-- test for bug reported in
-- CABRT9RC81YUf1=jsmWopcKJEro=VoeG2ou6sPwyOUTx_qteRsg@mail.gmail.com
CREATE TABLE IF NOT EXISTS testcase(
	id int PRIMARY KEY,
	balance numeric
);
INSERT INTO testcase VALUES (1, 0);
BEGIN;
SELECT * FROM testcase WHERE testcase.id = 1 FOR UPDATE;
UPDATE testcase SET balance = balance + 400 WHERE id=1;
SAVEPOINT subxact;
UPDATE testcase SET balance = balance - 100 WHERE id=1;
ROLLBACK TO SAVEPOINT subxact;
-- should return one tuple
SELECT * FROM testcase WHERE id = 1 FOR UPDATE;
ROLLBACK;
DROP TABLE testcase;
