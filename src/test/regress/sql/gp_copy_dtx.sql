-- Test if COPY FROM uses correct distribued transaction command protocol

CREATE TABLE test_copy_from_dtx (c1 int, c2 int);

SET Test_print_direct_dispatch_info = ON;

COPY test_copy_from_dtx (c1, c2) FROM stdin;
1	2
2	3
3	4
4	5
5	6
6	7
7	8
8	9
9	10
\.

SELECT * FROM test_copy_from_dtx;

SET Test_print_direct_dispatch_info = OFF;

-- Clean up
DROP TABLE test_copy_from_dtx;
