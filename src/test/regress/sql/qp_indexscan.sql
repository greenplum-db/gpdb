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

-- Tests for queries with order by and limit considering sort & null directions.
CREATE TABLE test_index_with_sort_directions_on_orderby_limit (a int, b text, c float, d int, e text, f int);
-- single col index with default order
CREATE INDEX dir_index_a on test_index_with_sort_directions_on_orderby_limit using btree(a);
-- single col index with reverse order
CREATE INDEX dir_index_b on test_index_with_sort_directions_on_orderby_limit using btree(b desc);
-- single col index with opp nulls direction
CREATE INDEX dir_index_c on test_index_with_sort_directions_on_orderby_limit using btree(c nulls first);
-- multi col index all with asc
CREATE INDEX dir_index_bcd on test_index_with_sort_directions_on_orderby_limit using btree(b,c,d);
-- multi col index all with desc
CREATE INDEX dir_index_fde on test_index_with_sort_directions_on_orderby_limit using btree(f desc,d desc,e desc);
-- multi col index mixed case
CREATE INDEX dir_index_eda on test_index_with_sort_directions_on_orderby_limit using btree(e, d desc nulls last,a);
-- Covering index
CREATE INDEX dir_covering_index_eb ON test_index_with_sort_directions_on_orderby_limit(e desc) INCLUDE (b);
INSERT INTO test_index_with_sort_directions_on_orderby_limit select i, CONCAT('col_b', i)::text, i/3.2, i+1, CONCAT('col_e', i)::text, i+3 from generate_series(1,10000) i;
INSERT INTO test_index_with_sort_directions_on_orderby_limit values (null, null, null, null, null);
ANALYZE test_index_with_sort_directions_on_orderby_limit;

-- should use Forward IndexScan
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a limit 3;
-- should use Backward IndexScan
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a desc limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a desc limit 3;
-- should use SeqScan with Sort
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a nulls first limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a nulls first limit 3;
explain (costs off) select a from test_index_with_sort_directions_on_orderby_limit order by a desc nulls last limit 3;
select a from test_index_with_sort_directions_on_orderby_limit order by a desc nulls last limit 3;

-- should use Forward IndexScan
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
-- should use Backward IndexScan
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b limit 3;
-- should use SeqScan with Sort
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b nulls first limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b nulls first limit 3;
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b desc nulls last limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b desc nulls last limit 3;

-- should use Forward IndexScan
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c nulls first limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c nulls first limit 3;
-- should use Backward IndexScan
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit 3;
-- should use SeqScan with Sort
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c  limit 3;
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc limit 3;
select c from test_index_with_sort_directions_on_orderby_limit order by c desc limit 3;

-- should use Forward IndexScan
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b,c,d limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b,c,d limit 3;
explain (costs off) select b,c from test_index_with_sort_directions_on_orderby_limit order by b,c limit 3;
select b,c from test_index_with_sort_directions_on_orderby_limit order by b,c limit 3;
-- should use Backward IndexScan
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc,d desc limit 3;
explain (costs off) select b,c from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc limit 3;
select b,c from test_index_with_sort_directions_on_orderby_limit order by b desc,c desc limit 3;
-- should use SeqScan with Sort
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c desc,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c desc,d desc limit 3;
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c ,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b ,c ,d desc limit 3;
explain (costs off) select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc, c ,d desc limit 3;
select b,c,d from test_index_with_sort_directions_on_orderby_limit order by b desc, c ,d desc limit 3;

-- should use Forward IndexScan
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc,e desc limit 3;
explain (costs off) select f,d from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc limit 3;
select f,d from test_index_with_sort_directions_on_orderby_limit order by f desc,d desc limit 3;
-- should use Backward IndexScan
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d,e limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d,e limit 3;
explain (costs off) select f,d from test_index_with_sort_directions_on_orderby_limit order by f,d limit 3;
select f,d from test_index_with_sort_directions_on_orderby_limit order by f,d limit 3;
-- should use SeqScan with Sort
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f ,d desc,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f ,d desc,e desc limit 3;
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d ,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f,d ,e desc limit 3;
explain (costs off) select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc, d ,e desc limit 3;
select f,d,e from test_index_with_sort_directions_on_orderby_limit order by f desc, d ,e desc limit 3;

-- should use Forward IndexScan
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last,a limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last,a limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
-- should use Backward IndexScan
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
explain (costs off) select e,d from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first limit 3;
select e,d from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first limit 3;
-- should use SeqScan with Sort
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc,a desc limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d desc,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d desc,a desc limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e ,d ,a  limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e ,d ,a  limit 3;

-- Backward indexscan with offset and without limit
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc offset 9990;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc offset 9997;
-- Backward indexscan with offset value in subquery
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last offset (select 9997);
select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last offset (select 9997);
-- Backward indexscan with limit value in subquery
explain (costs off) select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit (select 3);
select c from test_index_with_sort_directions_on_orderby_limit order by c desc nulls last limit (select 3);

-- order by on a non-index columns, should use SeqScan
explain (costs off) select d from test_index_with_sort_directions_on_orderby_limit order by d limit 3;
select d from test_index_with_sort_directions_on_orderby_limit order by d limit 3;
explain (costs off) select a,e from test_index_with_sort_directions_on_orderby_limit order by a,e limit 3;
select a,e from test_index_with_sort_directions_on_orderby_limit order by a,e limit 3;
explain (costs off) select d,a from test_index_with_sort_directions_on_orderby_limit order by d,a desc limit 3;
select d,a from test_index_with_sort_directions_on_orderby_limit order by d,a desc limit 3;
explain (costs off) select d,c from test_index_with_sort_directions_on_orderby_limit order by d desc,c limit 3;
select d,c from test_index_with_sort_directions_on_orderby_limit order by d desc,c limit 3;

-- order by on covering index, should use Backward IndexScan
explain (costs off) select e from test_index_with_sort_directions_on_orderby_limit order by e desc limit 3;
select e from test_index_with_sort_directions_on_orderby_limit order by e desc limit 3;

-- order by on covering index with included column, should use SeqScan
explain (costs off) select e,b from test_index_with_sort_directions_on_orderby_limit order by e, b limit 3;
select e,b from test_index_with_sort_directions_on_orderby_limit order by e,b limit 3;

-- check if IndexOnlyScan Forward/Backward is picked when required
set optimizer_enable_indexscan to off;
-- should use IndexOnlyScan Forward
explain (costs off) select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
select b from test_index_with_sort_directions_on_orderby_limit order by b desc limit 3;
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e, d desc nulls last limit 3;
-- should use IndexOnlyScan Backward
explain (costs off) select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
select e,d,a from test_index_with_sort_directions_on_orderby_limit order by e desc,d nulls first,a desc limit 3;
-- reset index scan
set optimizer_enable_indexscan to on;
DROP TABLE test_index_with_sort_directions_on_orderby_limit;

-- check if DynamicSeqScan is used for partition tables.
CREATE TABLE test_partition_table(a int, b int, c float) DISTRIBUTED BY (a) PARTITION BY range(a) (start (0) end(100) every(20));
CREATE INDEX part_index_a ON test_partition_table using btree(a);
INSERT INTO test_partition_table SELECT i, i+3, i/4.2 from generate_series(1,99) i;
ANALYZE test_partition_table;

explain (costs off) select a from test_partition_table order by a limit 3;
select a from test_partition_table order by a limit 3;