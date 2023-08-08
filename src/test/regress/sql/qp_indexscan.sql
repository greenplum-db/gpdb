-- Objective of these tests is to ensure if IndexScan is being picked up when order by clause has columns that match
-- prefix of any existing btree indices. This is for queries with both order by and a limit.

-- Tests for queries with order by and limit on B-tree indices.
CREATE TABLE test_index_with_orderby_limit (a int, b int, c float, d int);
CREATE INDEX index_a on test_index_with_orderby_limit using btree(a);
CREATE INDEX index_ab on test_index_with_orderby_limit using btree(a, b);
CREATE INDEX index_bda on test_index_with_orderby_limit using btree(b, d, a);
CREATE INDEX index_c on test_index_with_orderby_limit using hash(c);
INSERT INTO test_index_with_orderby_limit select i, i-2, i/3, i+1 from generate_series(1,10000) i;
ANALYZE test_index_with_orderby_limit;
-- should use index scan
explain (costs off) select a from test_index_with_orderby_limit order by a limit 10;
select a from test_index_with_orderby_limit order by a limit 10;
-- order by using a hash indexed column, should use SeqScan
explain (costs off) select c from test_index_with_orderby_limit order by c limit 10;
select c from test_index_with_orderby_limit order by c limit 10;
-- should use index scan
explain (costs off) select b from test_index_with_orderby_limit order by b limit 10;
select b from test_index_with_orderby_limit order by b limit 10;
-- should use index scan
explain (costs off) select a, b from test_index_with_orderby_limit order by a, b limit 10;
select a, b from test_index_with_orderby_limit order by a, b limit 10;
-- should use index scan
explain (costs off) select b, d from test_index_with_orderby_limit order by b, d limit 10;
select b, d from test_index_with_orderby_limit order by b, d limit 10;
-- should use seq scan
explain (costs off) select d, b from test_index_with_orderby_limit order by d, b limit 10;
select d, b from test_index_with_orderby_limit order by d, b limit 10;
-- should use seq scan
explain (costs off) select d, a from test_index_with_orderby_limit order by d, a limit 10;
select d, a from test_index_with_orderby_limit order by d, a limit 10;
-- should use seq scan
explain (costs off) select a, c from test_index_with_orderby_limit order by a, c limit 10;
select a, c from test_index_with_orderby_limit order by a, c limit 10;
-- should use index scan
explain (costs off) select b, d, a from test_index_with_orderby_limit order by b, d, a limit 10;
select b, d, a from test_index_with_orderby_limit order by b, d, a limit 10;
-- should use seq scan
explain (costs off) select b, d, c from test_index_with_orderby_limit order by b, d, c limit 10;
select b, d, c from test_index_with_orderby_limit order by b, d, c limit 10;
-- should use seq scan
explain (costs off) select c, b, a from test_index_with_orderby_limit order by c, b, a limit 10;
select c, b, a from test_index_with_orderby_limit order by c, b, a limit 10;
-- with offset and without limit
explain (costs off) select a  from test_index_with_orderby_limit order by a offset 9990;
select a  from test_index_with_orderby_limit order by a offset 9990;
-- limit value in subquery
explain (costs off) select a from test_index_with_orderby_limit order by a limit (select min(a) from test_index_with_orderby_limit);
select a from test_index_with_orderby_limit order by a limit (select min(a) from test_index_with_orderby_limit);
-- offset value in a subquery
explain (costs off) select c from test_index_with_orderby_limit order by c offset (select 9990);
select c from test_index_with_orderby_limit order by c offset (select 9990);
-- order by opposite to index sort direction
explain (costs off) select a from test_index_with_orderby_limit order by a desc limit 10;
select a from test_index_with_orderby_limit order by a desc limit 10;
-- order by opposite to nulls direction in index
explain (costs off) select a from test_index_with_orderby_limit order by a NULLS FIRST limit 10;
select a from test_index_with_orderby_limit order by a NULLS FIRST limit 10;
-- order by desc with nulls last
explain (costs off) select a from test_index_with_orderby_limit order by a desc NULLS LAST limit 10;
select a from test_index_with_orderby_limit order by a desc NULLS LAST limit 10;
-- order by as sum of two columns, uses SeqScan with Sort
explain (costs off) select a, b from test_index_with_orderby_limit order by a+b, c limit 3;
select a, b from test_index_with_orderby_limit order by a+b, c limit 3;
explain (costs off) select a+b as sum from test_index_with_orderby_limit order by sum limit 3;
select a+b as sum from test_index_with_orderby_limit order by sum limit 3;
-- order by using column number
explain (costs off) select a from test_index_with_orderby_limit order by 1 limit 3;
select a from test_index_with_orderby_limit order by 1 limit 3;
-- check if index-only scan is leveraged when required
set optimizer_enable_indexscan to off;
-- project only columns in the Index
explain (costs off) select b from test_index_with_orderby_limit order by b limit 10;
select b from test_index_with_orderby_limit order by b limit 10;
-- re-enable indexscan
set optimizer_enable_indexscan to on;
DROP TABLE test_index_with_orderby_limit;

-- Test Case: Test on a regular table with mixed data type columns.
-- Purpose: Validate if IndexScan with correct scan direction is used on expected index for queries with order by and limit.

CREATE TABLE test_index_with_sort_directions_on_orderby_limit (a int, b text, c float, d int, e text, f int);
-- single col index with default order
CREATE INDEX dir_index_a on test_index_with_sort_directions_on_orderby_limit using btree(a);
-- single col index with reverse order
CREATE INDEX dir_index_b on test_index_with_sort_directions_on_orderby_limit using btree(b desc);
-- single col index with opp nulls direction
CREATE INDEX dir_index_c on test_index_with_sort_directions_on_orderby_limit using btree(c nulls first);
-- multi col index all with all index keys asc
CREATE INDEX dir_index_bcd on test_index_with_sort_directions_on_orderby_limit using btree(b,c,d);
-- multi col index all with all index keys desc
CREATE INDEX dir_index_fde on test_index_with_sort_directions_on_orderby_limit using btree(f desc,d desc,e desc);
-- multi col index with mixed index keys properties
CREATE INDEX dir_index_eda on test_index_with_sort_directions_on_orderby_limit using btree(e, d desc nulls last,a);
-- Covering index with descending and one include column
CREATE INDEX dir_covering_index_db ON test_index_with_sort_directions_on_orderby_limit(d desc) INCLUDE (b);
INSERT INTO test_index_with_sort_directions_on_orderby_limit select i, CONCAT('col_b', i)::text, i/3.2, i+1, CONCAT('col_e', i)::text, i+3 from generate_series(1,10000) i;
-- Inserting nulls to verify results match when index key specifies nulls first or desc
INSERT INTO test_index_with_sort_directions_on_orderby_limit values (null, null, null, null, null);
ANALYZE test_index_with_sort_directions_on_orderby_limit;

-- Positive tests: Validate if IndexScan Forward/Backward is chosen.

-- Validate if 'dir_index_a' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward IndexScan
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a desc limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a desc limit 3;

-- Validate if 'dir_index_b' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward IndexScan
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b limit 3;

-- Validate if 'dir_index_c' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward IndexScan
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c nulls first limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c nulls first limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit 3;

-- Validate if 'dir_index_bcd' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward IndexScan
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b,c,d limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b,c,d limit 3;
explain (costs off) select b,c from test_index_with_sort_directions_on_orderby_limit order by b,c limit 3;
select b,c from test_index_with_sort_directions_on_orderby_limit order by b,c limit 3;
-- Testing various permutations of order by columns that are expected to choose Backward IndexScan
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc,d desc limit 3;
explain (costs off) select b,c from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc limit 3;
select b,c from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc limit 3;

-- Validate if 'dir_index_fde' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward IndexScan
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc,e desc limit 3;
explain (costs off) select f,d from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc limit 3;
select f,d from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc limit 3;
-- Testing various permutations of order by columns that are expected to choose Backward IndexScan
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d,e limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d,e limit 3;
explain (costs off) select f,d from test_index_with_sort_directions_on_orderby_limit order by f,d limit 3;
select f,d from test_index_with_sort_directions_on_orderby_limit order by f,d limit 3;

-- Validate if 'dir_index_eda' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward IndexScan
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last,a limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last,a limit 3;
explain (costs off) select e,d from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
select e,d from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
-- Testing various permutations of order by columns that are expected to choose Backward IndexScan
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
explain (costs off) select e,d from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first limit 3;
select e,d from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first limit 3;

-- Validate if IndexScan is chosen and on covering index
-- Expected to use Forward IndexScan
explain (costs off) select d from test_index_with_sort_directions_on_orderby_limit order by d desc limit 3;
select d from test_index_with_sort_directions_on_orderby_limit order by d desc limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select d from test_index_with_sort_directions_on_orderby_limit order by d limit 3;
select d from test_index_with_sort_directions_on_orderby_limit order by d limit 3;

-- Validate if Backward IndexScan is chosen for query with offset and without limit
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc offset 9990;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc offset 9997;

-- Validate if Backward IndexScan is chosen for query with offset value in subquery
-- ORCA_FEATURE_NOT_SUPPORTED: ORCA doesn't support limit or offset values specified as part of a subquery
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last offset (select 9997);
select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last offset (select 9997);
-- Validate if Backward IndexScan is chosen for query with limit value in subquery
-- ORCA_FEATURE_NOT_SUPPORTED: ORCA doesn't support limit or offset values specified as part of a subquery
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit (select 3);
select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit (select 3);

-- Negative tests: Validate if a SeqScan is chosen if order by cols directions do not matching indices keys directions.
--                 Expected to choose SeqScan with Sort

-- Testing various permutations that are not matching keys in 'dir_index_a'
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a nulls first limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a nulls first limit 3;
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a desc nulls last limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a desc nulls last limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_b'
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b nulls first limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b nulls first limit 3;
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b desc nulls last limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b desc nulls last limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_c'
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c  limit 3;
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c desc limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_bcd'
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c desc,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c desc,d desc limit 3;
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c ,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c ,d desc limit 3;
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc, c ,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc, c ,d desc limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_fde'
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f ,d desc,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f ,d desc,e desc limit 3;
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d ,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d ,e desc limit 3;
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc, d ,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc, d ,e desc limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_eda'
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc,a desc limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d desc,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d desc,a desc limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e ,d ,a  limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e ,d ,a  limit 3;

-- Testing various permutations of order by on non-index columns. Expected to choose SeqScan with Sort
explain (costs off) select d, f from test_index_with_sort_directions_on_orderby_limit order by d, f limit 3;
select d, f from test_index_with_sort_directions_on_orderby_limit order by d, f limit 3;
explain (costs off) select a,e from test_index_with_sort_directions_on_orderby_limit order by a,e limit 3;
select a,e from test_index_with_sort_directions_on_orderby_limit order by a,e limit 3;
explain (costs off) select d,a from test_index_with_sort_directions_on_orderby_limit order by d,a desc limit 3;
select d,a from test_index_with_sort_directions_on_orderby_limit order by d,a desc limit 3;
explain (costs off) select d,c from test_index_with_sort_directions_on_orderby_limit order by d desc,c limit 3;
select d,c from test_index_with_sort_directions_on_orderby_limit order by d desc,c limit 3;

-- Validate if SeqScan is chosen if order by cols also have the Included Column of covering index
explain (costs off) select e,b from test_index_with_sort_directions_on_orderby_limit order by e, b limit 3;
select e,b from test_index_with_sort_directions_on_orderby_limit order by e,b limit 3;

-- Purpose: Validate if IndexOnlyScan Forward/Backward is chosen when required for queries with order by and limit
-- Vacuum table to Ensure IndexOnlyScan is chosen
vacuum test_index_with_sort_directions_on_orderby_limit;
-- Testing various permutations of order by columns that are expected to choose IndexOnlyScan Forward
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
-- Testing various permutations of order by columns that are expected to choose IndexOnlyScan Backward
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;

-- Clean Up
DROP TABLE test_index_with_sort_directions_on_orderby_limit;


-- Test Case: Test on a Partition table with mixed data type columns.
-- Purpose: Validate if DynamicIndexScan/DynamicIndexOnlyScan with correct scan direction is used on expected index for queries with order by and limit.

CREATE TABLE test_partition_table(a int, b int, c float, d text, e numeric, f int) DISTRIBUTED BY (a) PARTITION BY range(a) (start (0) end(10000) every(2000));
-- single col index with opp nulls direction on partition column
CREATE INDEX part_index_a on test_partition_table using btree(a nulls first);
-- multi col index all with all index keys asc
CREATE INDEX part_index_bcd on test_partition_table using btree(b,c,d);
-- multi col index all with all index keys desc
CREATE INDEX part_index_fde on test_partition_table using btree(f desc,d desc,e desc);
-- multi col index with mixed index keys properties
CREATE INDEX part_index_eda on test_partition_table using btree(e desc nulls last, d,a desc);
-- Covering index on partition column
CREATE INDEX part_covering_index_cb ON test_partition_table(c desc) INCLUDE (b);
INSERT INTO test_partition_table SELECT i, i+3, i/4.2, concat('sample_text ',i), i/5, i from generate_series(1,9998) i;
-- Inserting nulls to verify results match when index key specifies nulls first or desc
INSERT INTO test_partition_table values (9999, null, null, null, null, null);
ANALYZE test_partition_table;

-- Positive tests: Validate if DynamicIndexScan Forward/Backward is chosen.
-- Using explain analyze to validate number of partitions scanned

-- Validate if 'part_index_a' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward DynamicIndexScan
explain analyze select a from test_partition_table order by a nulls first limit 3;
select a from test_partition_table order by a nulls first limit 3;
-- Expected to use Backward DynamicIndexScan
explain analyze select a from test_partition_table order by a desc nulls last limit 3;
select a from test_partition_table order by a desc nulls last limit 3;

-- Validate if 'part_index_bcd' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward DynamicIndexScan
explain analyze select b,c,d from test_partition_table order by b,c,d limit 3;
select b,c,d from test_partition_table order by b,c,d limit 3;
explain analyze select b,c from test_partition_table order by b,c limit 13;
select b,c from test_partition_table order by b,c limit 13;
-- Testing various permutations of order by columns that are expected to choose Backward DynamicIndexScan
explain analyze select b,c,d from test_partition_table order by b desc,c desc,d desc limit 3;
select b,c,d from test_partition_table order by b desc,c desc,d desc limit 3;
explain analyze select b,c from test_partition_table order by b desc,c desc limit 3;
select b,c from test_partition_table order by b desc,c desc limit 3;

-- Validate if 'part_index_fde' is used for order by cols matching/commutative to the index cols. Average partitions scanned
-- for this query are more as the limit value is higher.
-- Testing various permutations of order by columns that expected to choose Forward DynamicIndexScan
explain analyze select f,d,e from test_partition_table order by f desc,d desc,e desc limit 23;
select f,d,e from test_partition_table order by f desc,d desc,e desc limit 23;
explain analyze select f,d from test_partition_table order by f desc,d desc limit 3;
select f,d from test_partition_table order by f desc,d desc limit 3;
-- Testing various permutations of order by columns that should pick Backward DynamicIndexScan
explain analyze select f,d,e from test_partition_table order by f,d,e limit 3;
select f,d,e from test_partition_table order by f,d,e limit 3;
explain analyze select f,d from test_partition_table order by f,d limit 3;
select f,d from test_partition_table order by f,d limit 3;

-- Validate if 'part_index_eda' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that expected to choose Forward DynamicIndexScan
explain analyze select e,d,a from test_partition_table order by e desc nulls last, d, a desc limit 3;
select e,d,a from test_partition_table order by e desc nulls last, d, a desc limit 3;
explain analyze select e,d,a from test_partition_table order by e desc nulls last, d limit 3;
select e,d,a from test_partition_table order by e desc nulls last, d limit 3;
-- Testing various permutations of order by columns that expected to choose Backward DynamicIndexScan
explain analyze select e,d,a from test_partition_table order by e nulls first, d desc, a limit 3;
select e,d,a from test_partition_table order by e nulls first, d desc, a limit 3;
explain analyze select e,d from test_partition_table order by e nulls first, d desc limit 3;
select e,d from test_partition_table order by e nulls first, d desc limit 3;

-- Validate if DynamicIndexScan works on covering index
-- Expected to choose Forward DynamicIndexScan
explain analyze select c from test_partition_table order by c desc limit 3;
select c from test_partition_table order by c desc limit 3;
-- Expected to choose Backward DynamicIndexScan
explain analyze select c from test_partition_table order by c limit 3;
select c from test_partition_table order by c limit 3;

-- Negative tests: Validate if a DynamicSeqScan is chosen if order by cols directions do not matching indices keys directions.
--                 Expected to choose DynamicSeqScan with Sort

-- Testing various permutations that are not matching keys in 'part_index_a'
explain(costs off) select a from test_partition_table order by a limit 3;
select a from test_partition_table order by a  limit 3;

-- Testing various permutations that are not matching keys in 'part_index_bcd'
explain(costs off) select b,c,d from test_partition_table order by b ,c desc,d desc limit 3;
select b,c,d from test_partition_table order by b ,c desc,d desc limit 3;
explain(costs off) select b,c,d from test_partition_table order by b ,c ,d desc limit 3;
select b,c,d from test_partition_table order by b ,c ,d desc limit 3;
explain(costs off) select b,c,d from test_partition_table order by b desc, c ,d desc limit 3;
select b,c,d from test_partition_table order by b desc, c ,d desc limit 3;

-- Testing various permutations that are not matching keys in 'part_index_fde'
explain(costs off) select f,d,e from test_partition_table order by f ,d desc,e desc limit 3;
select f,d,e from test_partition_table order by f ,d desc,e desc limit 3;
explain(costs off) select f,d,e from test_partition_table order by f,d ,e desc limit 3;
select f,d,e from test_partition_table order by f,d ,e desc limit 3;
explain(costs off) select f,d,e from test_partition_table order by f desc, d ,e desc limit 3;
select f,d,e from test_partition_table order by f desc, d ,e desc limit 3;

-- Testing various permutations that are not matching keys in 'part_index_eda'
explain(costs off) select e,d,a from test_partition_table order by e, d desc,a desc limit 3;
select e,d,a from test_partition_table order by e, d desc,a desc limit 3;
explain(costs off) select e,d,a from test_partition_table order by e desc,d desc,a desc limit 3;
select e,d,a from test_partition_table order by e desc,d desc,a desc limit 3;
explain(costs off) select e,d,a from test_partition_table order by e ,d ,a  limit 3;
select e,d,a from test_partition_table order by e ,d ,a  limit 3;

-- -- Testing various permutations of order by on non-index columns. Expected to choose DynamicSeqScan with Sort
explain(costs off) select d from test_partition_table order by d limit 3;
select d from test_partition_table order by d limit 3;
explain(costs off) select a,e from test_partition_table order by a,e limit 3;
select a,e from test_partition_table order by a,e limit 3;
explain(costs off) select d,a from test_partition_table order by d,a desc limit 3;
select d,a from test_partition_table order by d,a desc limit 3;
explain(costs off) select d,c from test_partition_table order by d desc,c limit 3;
select d,c from test_partition_table order by d desc,c limit 3;

-- Purpose: Validate if DynamicIndexOnlyScan Forward/Backward is chosen when required for queries with order by and limit
-- Vacuum table to ensure DynamicIndexOnlyScans are choosen
vacuum test_partition_table;
-- Testing various permutations of order by columns that are expected to choose DynamicIndexOnlyScan Forward
explain analyze select a from test_partition_table order by a nulls first limit 3;
select a from test_partition_table order by a nulls first limit 3;
explain analyze select e,d,a from test_partition_table order by e desc nulls last, d limit 3;
select e,d,a from test_partition_table order by e desc nulls last, d limit 3;

-- Testing various permutations of order by columns that are expected to choose DynamicIndexOnlyScan Backward
explain analyze select a from test_partition_table order by a desc nulls last limit 3;
select a from test_partition_table order by a desc nulls last limit 3;
explain analyze select e,d,a from test_partition_table order by e nulls first, d desc, a limit 3;
select f,d,e from test_partition_table order by e nulls first, d desc, a limit 3;


-- Clean Up
DROP TABLE test_partition_table;


-- Test Case: Test on a Replicated table with mixed data type columns.
-- Purpose: Validate if Forward/Backward IndexScan works on Replicated table
CREATE TABLE test_replicated_table(a int, b int, c float, d text, e numeric) DISTRIBUTED REPLICATED;
-- multi col index with mixed index keys properties
CREATE INDEX rep_index_eda on test_replicated_table using btree(e desc nulls last, d,a desc);
INSERT INTO test_replicated_table SELECT i, i+3, i/4.2, concat('sample_text ',i), i/5 from generate_series(1,100) i;
-- Inserting nulls to verify results match when index key specifies nulls first or desc
INSERT INTO test_replicated_table values (null, null, null, null, null);

-- Positive tests: Validate if IndexScan Forward/Backward is chosen.

-- Validate if 'rep_index_eda' is used for order by matching to the index
explain(costs off) select e,d,a from test_replicated_table order by e desc nulls last, d, a desc limit 3;
select e,d,a from test_replicated_table order by e desc nulls last, d, a desc limit 3;
-- Validate if 'rep_index_eda' is used for order by commutative to the index
explain(costs off) select e,d,a from test_replicated_table order by e nulls first, d desc, a limit 3;
select e,d,a from test_replicated_table order by e nulls first, d desc, a limit 3;

-- Negative tests: Validate if a SeqScan is chosen for order by cols not matching any indices. Expected to choose SeqScan with Sort

explain(costs off) select d,a from test_replicated_table order by d,a desc limit 3;
select d,a from test_replicated_table order by d,a desc limit 3;

-- Clean Up
DROP TABLE test_replicated_table;



-- Test Case: Test on AO table with mixed data type columns.
-- ORCA_FEATURE_NOT_SUPPORTED: IndexScans not supported on AO tables
CREATE TABLE test_ao_table(a int, b int, c float, d text, e numeric) WITH (appendonly=true) DISTRIBUTED BY (a);
-- multi col index with mixed index keys properties
CREATE INDEX ao_index_eda on test_ao_table using btree(e desc nulls last, d,a desc);
INSERT INTO test_ao_table SELECT i, i+3, i/4.2, concat('sample_text ',i), i/5 from generate_series(1,100) i;

-- Expected to choose SeqScan with a Sort as it is an AO table
explain(costs off) select e,d,a from test_ao_table order by e desc nulls last, d, a desc limit 3;
select e,d,a from test_ao_table order by e desc nulls last, d, a desc limit 3;

-- Clean Up
DROP TABLE test_ao_table;



-- Test Case: Test on table with all other types of indexes apart from btree(bitmap, hash, brin, spgist, gist, gin)
-- Purpose: Evaluate if Forward/Backward IndexScan works on query with order by and limit, with other type of indices
-- Note: No other index type apart from btree support IndexScans
CREATE TABLE test_multi_index_types_table(a int, b int, c float, d text, e tsquery, f tsvector);
-- create a bitmap index
create index bitmap_a on test_multi_index_types_table using bitmap(a);
-- create a hash index
create index hash_b on test_multi_index_types_table using hash(b);
-- create a brin index
create index brin_c on test_multi_index_types_table using brin(c);
-- create a spgist index
create index spgist_d on test_multi_index_types_table using spgist(d);
-- create a gin index
create index gist_e on test_multi_index_types_table using gist(e);
-- create a gin index
create index gin_f on test_multi_index_types_table using gin(f);
-- All of the below queries are expected to use SeqScan with a Sort as only btree index supports IndexScan
explain(costs off) select a from test_multi_index_types_table order by a limit 3;
explain(costs off) select b from test_multi_index_types_table order by b limit 3;
explain(costs off) select c from test_multi_index_types_table order by c limit 3;
explain(costs off) select d from test_multi_index_types_table order by d limit 3;
explain(costs off) select e from test_multi_index_types_table order by e limit 3;
explain(costs off) select f from test_multi_index_types_table order by f limit 3;

-- Clean Up
DROP TABLE test_multi_index_types_table;


-- Purpose: Test Forward/Backward IndexScan over views
create table test_on_views(a int, b int, c float);
INSERT INTO test_on_views SELECT i+3, i, i/4.2 from generate_series(1,100) i;
-- create a index on column b
create index view_index on test_on_views using btree(b);
analyze test_on_views;
-- create view
create view test_view as select b from test_on_views;
-- Expected to use IndexScan Forward
explain(costs off) select * from test_view order by b limit 3;
select * from test_view order by b limit 3;
-- Expected to use IndexScan Backwards
explain(costs off) select * from test_view order by b desc limit 3;
select * from test_view order by b desc limit 3;
-- Clean Up
DROP VIEW test_view;
DROP TABLE test_on_views;

-- Purpose: Test Forward/Backward IndexScan over partial indices
-- ORCA_FEATURE_NOT_SUPPORTED: partial indexes are not supported
create table test_on_partial_indices(a int, b int, c float);
-- create a partial index on column b
create index partial_index on test_on_partial_indices(b desc) where b<54;
analyze test_on_partial_indices;
-- Expected to use SeqScan with Sort
explain(costs off) select b from test_on_partial_indices order by b desc limit 3;
-- Clean Up
DROP TABLE test_on_partial_indices;

-- Purpose: Test Forward/Backward IndexScan over primary key
create table test_on_pk_column(a int primary key , b int, c float);
INSERT INTO test_on_pk_column SELECT i+3, i, i/4.2 from generate_series(1,100) i;
analyze test_on_pk_column;
-- Expected to use Forward IndexScan
explain(costs off) select a from test_on_pk_column order by a limit 3;
select a from test_on_pk_column order by a limit 3;
-- Expected to use Backward IndexScan
explain(costs off) select a from test_on_pk_column order by a desc limit 3;
select a from test_on_pk_column order by a desc limit 3;
-- Clean Up
DROP TABLE test_on_pk_column;

-- Purpose: Test Forward/Backward IndexScan over column with unique constraint
create table test_on_unique_column(a int, b int unique, c float);
INSERT INTO test_on_unique_column SELECT i+3, i, i/4.2 from generate_series(1,100) i;
analyze test_on_unique_column;
-- Expected to use Forward IndexScan
explain(costs off) select a from test_on_unique_column order by b limit 3;
select a from test_on_unique_column order by b limit 3;
-- Expected to use Backward IndexScan
explain(costs off) select a from test_on_unique_column order by b desc limit 3;
select a from test_on_unique_column order by b desc limit 3;
-- Clean Up
DROP TABLE test_on_unique_column;

-- Purpose: Test Forward/Backward IndexScan with order by on Index Expressions
-- ORCA_FEATURE_NOT_SUPPORTED: Indexes on Expressions are not supported by ORCA
create table test_on_index_expressions(a int, b int, c float);
CREATE INDEX expr_index_a on test_on_index_expressions using btree(a);
analyze test_on_index_expressions;
-- Expected to use SeqScan with Sort
explain(costs off) select a,b from test_on_index_expressions order by a*b desc limit 3;
-- Expected to use SeqScan with Sort
explain(costs off) select a from test_on_index_expressions order by a|2 limit 3;
-- Expected to use SeqScan with Sort
explain(costs off) select a from test_on_index_expressions order by a is not null desc limit 3;
-- Expected to use SeqScan with Sort
explain(costs off) select a from test_on_index_expressions order by a>3 limit 3;
-- define a simple multiplication function
CREATE OR REPLACE FUNCTION multiply_by_two(integer)
RETURNS INTEGER
LANGUAGE 'plpgsql'
AS $$
BEGIN
RETURN $1 * 2;
END;
$$;
-- Order by using multiplication function. Expected to use SeqScan with Sort
explain(costs off) select a from test_on_index_expressions order by multiply_by_two(a) limit 3;
-- Clean Up
DROP FUNCTION multiply_by_two;
DROP TABLE test_on_index_expressions;

-- Purpose: Test Forward/Backward IndexScan with order by on custom data type
-- create a custom type
CREATE TYPE custom_data_type AS (
    name VARCHAR,
    age INTEGER);
create table test_on_custom_data_type(a int, b float, c custom_data_type);
create index index_on_custom_type on test_on_custom_data_type using btree(c);
insert into test_on_custom_data_type select i, i/3, (concat('person', i), i)::custom_data_type from generate_series(1,100)i;
analyze test_on_custom_data_type;
-- Expected to use Forward IndexScan
explain(costs off) select c from test_on_custom_data_type order by c limit 3;
select c from test_on_custom_data_type order by c limit 3;
-- Expected to use Backward IndexScan
explain(costs off) select c from test_on_custom_data_type order by c desc limit 3;
select c from test_on_custom_data_type order by c desc limit 3;
-- Clean Up
DROP TABLE test_on_custom_data_type;
DROP TYPE custom_data_type;


-- Purpose: This section includes tests on general table where backward index scan could be used, but is not used currently since
--          those cases are not supported as part of this initial addition of backward index support.
CREATE TABLE test_yet_unsupported_backwrd_idxscan_cases (a int, b text, c float, d int, e int);
-- single col index with default order
CREATE INDEX index_a on test_yet_unsupported_backwrd_idxscan_cases using btree(a);
-- single col index with reverse order
CREATE INDEX index_b on test_yet_unsupported_backwrd_idxscan_cases using btree(b desc);
CREATE INDEX index_cd on test_yet_unsupported_backwrd_idxscan_cases using btree(c, d);
-- Inserting data to demonstrate that Planner chooses IndexScans for these cases
INSERT INTO test_yet_unsupported_backwrd_idxscan_cases select i, concat('sample_text', i), i/3.3, i,i-2 from generate_series(1,10000)i;
ANALYZE test_yet_unsupported_backwrd_idxscan_cases;

-- Cases with just order by without limit
explain(costs off) select a from test_yet_unsupported_backwrd_idxscan_cases order by a desc;

explain(costs off) select c,d from test_yet_unsupported_backwrd_idxscan_cases order by c desc, d desc;

-- Since col a is asc in index, max(a) could use a backward index scan
explain(costs off) select max(a) from test_yet_unsupported_backwrd_idxscan_cases;

-- Cases with a predicate and order by (with/without limit). Order by columns commutating index column
explain(costs off) select * from test_yet_unsupported_backwrd_idxscan_cases where a>997 order by c desc, d desc;

explain(costs off) select * from test_yet_unsupported_backwrd_idxscan_cases where a>997 order by c desc, d desc limit 3;

-- Cases with group by, order by (with/without limit). Order by cols commutating index column
explain(costs off) select  a, sum(d) from test_yet_unsupported_backwrd_idxscan_cases group by a order by a desc;

explain(costs off) select  a, sum(d) from test_yet_unsupported_backwrd_idxscan_cases group by a order by a desc limit 3;

-- Case with group by, order by and a having clause (with/without limit). Order by cols commutating index.
explain(costs off) select  a, sum(d) from test_yet_unsupported_backwrd_idxscan_cases group by a having a>30 order by a desc;

explain(costs off) select  a, sum(d) from test_yet_unsupported_backwrd_idxscan_cases group by a having a>30 order by a desc;

-- Case with ordering via over() using window aggregates (with/without limit): rank(), row_number(), percent_rank() etc...
explain(costs off) select c,d, rank() over (order by c desc, d desc) from test_yet_unsupported_backwrd_idxscan_cases;

explain(costs off) select c,d, rank() over (order by c desc, d desc) from test_yet_unsupported_backwrd_idxscan_cases limit 3;

-- Case with distinct and order by (with/without limit)
explain(costs off) select distinct(a) from test_yet_unsupported_backwrd_idxscan_cases order by a desc;

explain(costs off) select distinct(a) from test_yet_unsupported_backwrd_idxscan_cases order by a desc limit 3;

-- Order by within a CTE without limit
explain(costs off) with sorted_by_cd as (select c,d from test_yet_unsupported_backwrd_idxscan_cases order by c desc, d desc) select c from sorted_by_cd;

-- Order by within a CTE, with limit outside CTE expression
explain(costs off) with sorted_by_cd as (select c,d from test_yet_unsupported_backwrd_idxscan_cases order by c desc, d desc) select c from sorted_by_cd limit 3;

-- Clean Up
DROP TABLE test_yet_unsupported_backwrd_idxscan_cases;

-- Test Case: NL Joins can utilize IndexScan's sort property, but currently ORCA doesn't generate IndexScan alternatives
--            for NL joins. This tests the case where IndexScan's order property could be used for joining two tables
CREATE TABLE employee(id int, name text, dep_id int, salary int);
CREATE TABLE department(dep_id int, dep_name text);
CREATE INDEX index_salary on employee using btree(salary);
ANALYZE employee;
ANALYZE department;
-- Forcing planner, ORCA to use a NL join
set enable_hashjoin to off;
set optimizer_enable_hashjoin to off;
-- Planner uses NL Join with IndexScan Backwards and the sort property of index 'index_salary',
-- but ORCA, since it doesn't generate IndexScan alternative uses NL join with a Sort operator.
explain(costs off) select e.id, e.name, e.salary, d.dep_name from employee e join department d on e.id=d.dep_id order by e.salary desc;
explain(costs off) select e.id, e.name, e.salary, d.dep_name from employee e join department d on e.id=d.dep_id order by e.salary desc limit 3;
-- Clean up
reset enable_hashjoin;
reset optimizer_enable_hashjoin;
DROP TABLE employee;
DROP TABLE department;


-- Test Case: Union all of two tables with order by on their indexed column uses IndexScan's sort property with MergeAppend node
--            in Planner. However in ORCA we don't generate IndexScan alternative for union all, also we don't support MergeAppend.
--            But documenting this case for reference
CREATE TABLE table1(a int, b int);
CREATE TABLE table2(a int, b int);
CREATE INDEX tab1_idx on table1 using btree(b);
CREATE INDEX tab2_idx on table2 using btree(b);
-- inserting data and disabling seq_scan to avoid Planner generating a plan with Sort operator and Append node
-- instead of IndexScan with MergeAppend
set enable_seqscan to off;
INSERT INTO table1 select i, i from generate_series(1,99)i;
INSERT INTO table2 select i, i from generate_series(1,99)i;
ANALYZE table1;
ANALYZE table2;
explain(costs off) select b from table1 union all select b from table2 order by b desc;
reset enable_seqscan;
DROP TABLE table1;
DROP TABLE table2;


-- Purpose: This section includes tests on partition table where backward DynamicIndexScan could be used, but is not used currently since
--          those cases are not supported as part of this initial addition of backward index support.
CREATE TABLE test_partition_table(a int, b text, c float, d int, e int) DISTRIBUTED BY (a) PARTITION BY range(a) (start (0) end(10000) every(2000));
-- single col index with opp nulls direction on partition column
CREATE INDEX part_index_a on test_partition_table using btree(a);
-- multi col index all with all index keys asc
CREATE INDEX part_index_b on test_partition_table using btree(b desc);
-- multi col index all with all index keys desc
CREATE INDEX part_index_cd on test_partition_table using btree(c,d);
-- Inserting data to demonstrate that Planner chooses IndexScans for these cases
INSERT INTO test_partition_table select i, concat('sample_text', i), i/3.3, i,i-2 from generate_series(1,9999)i;
ANALYZE test_partition_table;

-- Cases with just order by without limit
explain(costs off) select a from test_partition_table order by a desc;

explain(costs off) select c,d from test_partition_table order by c desc, d desc;

-- Since col a is asc in index, max(a) could use a backward index scan
explain(costs off) select max(a) from test_partition_table;

-- Cases with a predicate and order by (with/without limit). Order by columns commutating index column
explain(costs off) select * from test_partition_table where a BETWEEN 40 and 4000 or c not between 4000 and 6000 order by c desc, d desc;

explain(costs off) select * from test_partition_table where a>7 order by c desc, d desc limit 4;

-- Cases with group by, order by (with/without limit). Order by cols commutating index column
explain(costs off) select  a, sum(d) from test_partition_table group by a order by a desc;

explain(costs off) select  a, sum(d) from test_partition_table group by a order by a desc limit 3;

-- Case with group by, order by and a having clause (with/without limit). Order by cols commutating index.
explain(costs off) select  a, sum(d) from test_partition_table group by a having a>30 order by a desc;

explain(costs off) select  a, sum(d) from test_partition_table group by a having a>30 order by a desc;

-- Case with ordering via over() using window aggregates (with/without limit): rank(), row_number(), percent_rank() etc...
explain(costs off) select c,d, rank() over (order by c desc, d desc) from test_partition_table;

explain(costs off) select c,d, rank() over (order by c desc, d desc) from test_partition_table limit 3;

-- Case with distinct and order by (with/without limit)
explain(costs off) select distinct(a) from test_partition_table order by a desc;

explain(costs off) select distinct(a) from test_partition_table order by a desc limit 3;

-- Order by within a CTE without limit
explain(costs off) with sorted_by_cd as (select c,d from test_partition_table order by c desc, d desc) select c from sorted_by_cd;

-- Order by within a CTE, with limit outside CTE expression
explain(costs off) with sorted_by_cd as (select c,d from test_partition_table order by c desc, d desc) select c from sorted_by_cd limit 3;
-- Clean Up
DROP TABLE test_partition_table;

-- Test Case: NL Joins can utilize IndexScan's sort property, but currently ORCA doesn't generate IndexScan alternatives for NL joins.
--            This tests the case where IndexScan's order property could be used for joining two partition tables
CREATE TABLE part_employee(id int, name text, dep_id int, salary int) PARTITION BY range(id) (start (0) end(10000) every(2000));
CREATE TABLE part_department(dep_id int, dep_name text) PARTITION BY range(dep_id) (start (0) end(10000) every(2000));
CREATE INDEX part_index_salary on part_employee using btree(salary);
ANALYZE part_employee;
ANALYZE part_department;
-- Forcing planner, ORCA to use a NL join
set enable_hashjoin to off;
set optimizer_enable_hashjoin to off;
-- Planner uses NL Join with IndexScan Backwards and the sort property of index 'index_salary',
-- but ORCA since doesn't generate IndexScan alternative uses NL join with a Sort operator.
explain(costs off) select e.id, e.name, e.salary, d.dep_name from part_employee e join part_department d on e.id=d.dep_id order by e.salary desc;
explain(costs off) select e.id, e.name, e.salary, d.dep_name from part_employee e join part_department d on e.id=d.dep_id order by e.salary desc limit 3;
-- Clean up
reset enable_hashjoin;
reset optimizer_enable_hashjoin;
DROP TABLE part_employee;
DROP TABLE part_department;

-- Test Case: Union all of two partition tables with order by on their indexed column uses IndexScan's sort property with MergeAppend node
--            in Planner. However in ORCA we don't generate DynamicIndexScan alternative for union all, also we don't support MergeAppend.
--            But documenting this case for reference
CREATE TABLE part_table1(a int, b int) PARTITION BY range(a) (start (0) end(100) every(20));
CREATE TABLE part_table2(a int, b int) PARTITION BY range(a) (start (0) end(100) every(20));
CREATE INDEX part_tab1_idx on part_table1 using btree(b);
CREATE INDEX part_tab2_idx on part_table2 using btree(b);
-- inserting data and disabling seq_scan to avoid Planner generating a plan with Sort operator and Append node
-- instead of IndexScan with MergeAppend
set enable_seqscan to off;
INSERT INTO part_table1 select i, i from generate_series(1,99)i;
INSERT INTO part_table2 select i, i from generate_series(1,99)i;
ANALYZE part_table1;
ANALYZE part_table2;
explain(costs off) select b from part_table1 union all select b from part_table2 order by b desc;
-- Clean Up
reset enable_seqscan;
DROP TABLE part_table1;
DROP TABLE part_table2;