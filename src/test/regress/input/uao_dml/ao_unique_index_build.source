-- Test cases to cover CREATE UNIQUE INDEX on AO/CO tables.

SET default_table_access_method TO @amname@;
-- To force index scans for smoke tests
SET enable_seqscan TO off;
SET optimizer TO off;

-- Case 1: Build with no conflicting rows.
CREATE TABLE unique_index_build_@amname@(i int) DISTRIBUTED REPLICATED;
INSERT INTO unique_index_build_@amname@ VALUES(1);
-- should succeed
CREATE UNIQUE INDEX on unique_index_build_@amname@(i);
-- post-build smoke test
EXPLAIN SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
INSERT INTO unique_index_build_@amname@ VALUES(1);

DROP TABLE unique_index_build_@amname@;

-- Case 2: Build with conflicting row.
CREATE TABLE unique_index_build_@amname@(i int) DISTRIBUTED REPLICATED;
INSERT INTO unique_index_build_@amname@ VALUES(1);
INSERT INTO unique_index_build_@amname@ VALUES(1);
-- should ERROR out
CREATE UNIQUE INDEX on unique_index_build_@amname@(i);

DROP TABLE unique_index_build_@amname@;

-- Case 3: Build with conflict on aborted row.
CREATE TABLE unique_index_build_@amname@(i int) DISTRIBUTED REPLICATED;
BEGIN;
INSERT INTO unique_index_build_@amname@ VALUES(1);
ABORT;
INSERT INTO unique_index_build_@amname@ VALUES(1);
-- should succeed
CREATE UNIQUE INDEX on unique_index_build_@amname@(i);
-- post-build smoke test
EXPLAIN SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
INSERT INTO unique_index_build_@amname@ VALUES(1);

DROP TABLE unique_index_build_@amname@;

-- Case 4: Build with conflict on deleted row.
CREATE TABLE unique_index_build_@amname@(i int) DISTRIBUTED REPLICATED;
INSERT INTO unique_index_build_@amname@ VALUES(1);
DELETE FROM unique_index_build_@amname@;
INSERT INTO unique_index_build_@amname@ VALUES(1);
-- should succeed
CREATE UNIQUE INDEX on unique_index_build_@amname@(i);
-- post-build smoke test
EXPLAIN SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
INSERT INTO unique_index_build_@amname@ VALUES(1);

DROP TABLE unique_index_build_@amname@;

-- Case 5: Partial unique index build
CREATE TABLE unique_index_build_@amname@(i int) DISTRIBUTED REPLICATED;
INSERT INTO unique_index_build_@amname@ VALUES(1);
INSERT INTO unique_index_build_@amname@ VALUES(1);
INSERT INTO unique_index_build_@amname@ VALUES(2);
INSERT INTO unique_index_build_@amname@ VALUES(6);
INSERT INTO unique_index_build_@amname@ VALUES(6);
-- should fail as conflict lies in indexed portion of data
CREATE UNIQUE INDEX on unique_index_build_@amname@(i) WHERE i < 5;
-- removing conflict should make index build succeed
DELETE FROM unique_index_build_@amname@ WHERE i = 1;
CREATE UNIQUE INDEX on unique_index_build_@amname@(i) WHERE i < 5;
-- post build smoke tests:
-- should succeed as it lies in non-indexed portion
INSERT INTO unique_index_build_@amname@ VALUES(6);
-- should fail as conflict lies in indexed portion of data
INSERT INTO unique_index_build_@amname@ VALUES(2);
-- should succeed as there is no conflicting key that exists
INSERT INTO unique_index_build_@amname@ VALUES(3);
SELECT * FROM unique_index_build_@amname@ WHERE i = 1;
SELECT * FROM unique_index_build_@amname@ WHERE i = 2;
SELECT * FROM unique_index_build_@amname@ WHERE i = 3;
SELECT * FROM unique_index_build_@amname@ WHERE i = 6;

DROP TABLE unique_index_build_@amname@;

RESET default_table_access_method;
RESET enable_seqscan;
RESET optimizer;
