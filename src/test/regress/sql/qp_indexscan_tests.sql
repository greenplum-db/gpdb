
-- Tests for queries with order by and limit on B-tree indices.
CREATE TABLE test_index_with_orderby_limit (a int, b int, c float, d int);
CREATE INDEX index_a on test_index_with_orderby_limit using btree(a);
CREATE INDEX index_ab on test_index_with_orderby_limit using btree(a, b);
CREATE INDEX index_bda on test_index_with_orderby_limit using btree(b, d, a);
INSERT INTO test_index_with_orderby_limit select i, i-2, i/3, i+1 from generate_series(1,10000) i;
ANALYZE test_index_with_orderby_limit;
-- should use index scan
explain (costs off) select * from test_index_with_orderby_limit order by a limit 10;
-- should use seq scan
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
-- check if index-only scan is leveraged when required
-- vacuum table to ensure IndexOnly Scan is picked
VACUUM test_index_with_orderby_limit;
-- project only columns in the Index
explain (costs off) select a from test_index_with_orderby_limit order by a limit 10;

DROP TABLE test_index_with_orderby_limit;