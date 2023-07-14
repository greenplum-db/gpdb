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
explain (costs off) select * from test_index_with_orderby_limit order by a limit 10;
-- order by using a hash indexed column, should use SeqScan
explain (costs off) select * from test_index_with_orderby_limit order by c limit 10;
-- should use index scan
explain (costs off) select * from test_index_with_orderby_limit order by b limit 10;
-- should use index scan
explain (costs off) select * from test_index_with_orderby_limit order by a, b limit 10;
-- should use index scan
explain (costs off) select * from test_index_with_orderby_limit order by b, d limit 10;
-- should use seq scan
explain (costs off) select * from test_index_with_orderby_limit order by d, b limit 10;
-- should use seq scan
explain (costs off) select * from test_index_with_orderby_limit order by d, a limit 10;
-- should use seq scan
explain (costs off) select * from test_index_with_orderby_limit order by a, c limit 10;
-- should use index scan
explain (costs off) select * from test_index_with_orderby_limit order by b, d, a limit 10;
-- should use seq scan
explain (costs off) select * from test_index_with_orderby_limit order by b, d, c limit 10;
-- should use seq scan
explain (costs off) select * from test_index_with_orderby_limit order by c, b, a limit 10;
-- with offset and without limit
explain (costs off) select * from test_index_with_orderby_limit order by a offset 10;
-- limit value in subquery
explain (costs off) select * from test_index_with_orderby_limit order by a limit (select min(a) from test_index_with_orderby_limit);
-- offset value in a subquery
explain (costs off) select * from test_index_with_orderby_limit order by c offset (select min(a) from test_index_with_orderby_limit);
-- order by opposite to index sort direction
explain (costs off) select * from test_index_with_orderby_limit order by a desc limit 10;
-- order by opposite to nulls direction in index
explain (costs off) select * from test_index_with_orderby_limit order by a NULLS FIRST limit 10;
-- order by as sum of two columns, uses SeqScan with Sort
explain (costs off) select a, b from test_index_with_orderby_limit order by a+b, c limit 3;
explain (costs off) select a+b as sum from test_index_with_orderby_limit order by sum limit 3;
-- order by using column number
explain (costs off) select * from test_index_with_orderby_limit order by 1 limit 3;
-- check if index-only scan is leveraged when required
set optimizer_enable_indexscan to off;
-- project only columns in the Index
explain (costs off) select b from test_index_with_orderby_limit order by b limit 10;

DROP TABLE test_index_with_orderby_limit;