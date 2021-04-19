-- This test is used to test the dispatch of the can on replicated table

CREATE TABLE test_rep_dispatch_rep (c1 int, c2 int) DISTRIBUTED REPLICATED;
CREATE TABLE test_rep_dispatch_normal (c1 int, c2 int) DISTRIBUTED BY (c1);

INSERT INTO test_rep_dispatch_rep values (1,1),(2,2),(3,3);
INSERT INTO test_rep_dispatch_normal values (1,2),(2,3),(5,6);


SELECT gp_segment_id, * FROM test_rep_dispatch_normal;

-- Test 3 times to make sure with the same gp_session_id, but update the tuples of
-- different segmets, all the 3 DTX will use 1PC.
SET Test_print_direct_dispatch_info = ON;
SET optimizer = OFF;
BEGIN;
UPDATE test_rep_dispatch_normal set c2 = 1 where c1 = 1;
SELECT * FROM test_rep_dispatch_rep;
COMMIT;

BEGIN;
UPDATE test_rep_dispatch_normal set c2 = 2 where c1 = 2;
SELECT * FROM test_rep_dispatch_rep;
COMMIT;

BEGIN;
UPDATE test_rep_dispatch_normal set c2 = 5 where c1 = 5;
SELECT * FROM test_rep_dispatch_rep;
COMMIT;

-- Clean up
DROP TABLE test_rep_dispatch_rep;
DROP TABLE test_rep_dispatch_normal;
