-- Test the built-in gp_dump_query function.
SELECT gp_dump_query_oids('SELECT 123');
SELECT gp_dump_query_oids('SELECT * FROM pg_proc');
SELECT gp_dump_query_oids('SELECT length(proname) FROM pg_proc');

-- with EXPLAIN
SELECT gp_dump_query_oids('explain SELECT length(proname) FROM pg_proc');

-- with a multi-query statement
SELECT gp_dump_query_oids('SELECT length(proname) FROM pg_proc; SELECT abs(relpages) FROM pg_class');

-- Test error reporting on an invalid query.
SELECT gp_dump_query_oids('SELECT * FROM nonexistent_table');
SELECT gp_dump_query_oids('SELECT with syntax error');

-- Test partition tables
CREATE TABLE minirepro_partition_test (id int, info json);
CREATE TABLE p3_sales (id int, year int, a int, b int, c int, d int, region text)
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (year)
     SUBPARTITION BY RANGE (a)
        SUBPARTITION TEMPLATE (
         START (1) END (3) EVERY (1),
         DEFAULT SUBPARTITION other_a )
            SUBPARTITION BY RANGE (b)
               SUBPARTITION TEMPLATE (
               START (1) END (3) EVERY (1),
               DEFAULT SUBPARTITION other_b )
( START (2002) END (2012) EVERY (1),
   DEFAULT PARTITION outlying_years );
INSERT INTO minirepro_partition_test VALUES (1, (select gp_dump_query_oids('SELECT * FROM p3_sales') ) :: json);
INSERT INTO minirepro_partition_test VALUES (2, (select gp_dump_query_oids('SELECT * FROM p3_sales_1_prt_11_2_prt_3') ) :: json);
INSERT INTO minirepro_partition_test VALUES (3, (select gp_dump_query_oids('SELECT * FROM pg_class') ) :: json);
SELECT array_length((SELECT string_to_array((SELECT info->>'relids' FROM minirepro_partition_test WHERE id = 1),',')),1);
SELECT array_length((SELECT string_to_array((SELECT info->>'relids' FROM minirepro_partition_test WHERE id = 2),',')),1);
SELECT array_length((SELECT string_to_array((SELECT info->>'relids' FROM minirepro_partition_test WHERE id = 3),',')),1);
DROP TABLE p3_sales;
DROP TABLE minirepro_partition_test;
