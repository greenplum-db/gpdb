
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

DROP TABLE test_index_with_orderby_limit;