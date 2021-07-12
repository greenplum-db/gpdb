-- This test is used to test if the segments involved in a dtx is correct

CREATE TABLE test_dispatch_normal (c1 int, c2 int) DISTRIBUTED BY (c1);

-- Case 1: replicated table 

CREATE TABLE test_dispatch_rep (c1 int, c2 int) DISTRIBUTED REPLICATED;

INSERT INTO test_dispatch_rep values (1,1),(2,2),(3,3);
INSERT INTO test_dispatch_normal values (1,2),(2,3),(5,6);


SELECT gp_segment_id, * FROM test_dispatch_normal;

-- Test 3 times to make sure with the same gp_session_id, but update the tuples of
-- different segmets, all the 3 DTX will use 1PC, and dispatch commit message to only
-- 1 segment.
SET Test_print_direct_dispatch_info = ON;
SET optimizer = OFF;

BEGIN;
UPDATE test_dispatch_normal SET c2 = 1 WHERE c1 = 1;
SELECT * FROM test_dispatch_rep;
COMMIT;

BEGIN;
UPDATE test_dispatch_normal SET c2 = 2 WHERE c1 = 2;
SELECT * FROM test_dispatch_rep;
COMMIT;

BEGIN;
UPDATE test_dispatch_normal SET c2 = 5 WHERE c1 = 5;
SELECT * FROM test_dispatch_rep;
COMMIT;

-- Case 2: Normal table
SET Test_print_direct_dispatch_info = ON;
SET optimizer = OFF;

-- 1PC, dispatch commit message to 1 segments 
BEGIN;
UPDATE test_dispatch_normal SET c2 = 1 WHERE c1 = 1;
SELECT * FROM test_dispatch_normal WHERE c1 = 1;
COMMIT;

-- 1PC, but dispatch commit message to 2 segments
BEGIN;
UPDATE test_dispatch_normal SET c2 = 1 WHERE c1 = 1;
SELECT * FROM test_dispatch_normal WHERE c1 = 1 or c1 = 2;
COMMIT;

-- 1PC, but dispatch commit message to all segments
BEGIN;
UPDATE test_dispatch_normal SET c2 = 1 WHERE c1 = 1;
SELECT * FROM test_dispatch_normal;
COMMIT;

-- 2PC, dispatch prepare message to 2 segments, dispatch commit prepared message to 2 segments
-- dispatch commit one-phase message to 1 segment
BEGIN;
UPDATE test_dispatch_normal SET c2 = 1 WHERE c1 = 1;
UPDATE test_dispatch_normal SET c2 = 2 WHERE c1 = 2;
SELECT * FROM test_dispatch_normal WHERE c1 = 5; 
COMMIT;

-- 2PC, dispatch prepare and commit message to all segments
BEGIN;
UPDATE test_dispatch_normal SET c2 = 1 WHERE c1 = 1;
UPDATE test_dispatch_normal SET c2 = 2 WHERE c1 = 2;
UPDATE test_dispatch_normal SET c2 = 5 WHERE c1 = 5;
SELECT * FROM test_dispatch_normal WHERE c1 = 5; 
COMMIT;

-- Clean up
DROP TABLE test_dispatch_rep;
DROP TABLE test_dispatch_normal;
