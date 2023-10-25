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
reset optimizer_enable_indexscan;
DROP TABLE test_index_with_orderby_limit;

-- Test Case: Test on a regular table with mixed data type columns.
-- Purpose: Validate if IndexScan with correct scan direction is used on expected index for queries with order by and limit.

CREATE TABLE zoo (a int, b text, c float, d int, e text, f int);
INSERT INTO zoo select i, CONCAT('col_b', i)::text, i/3.2, i+1, CONCAT('col_e', i)::text, i+3 from generate_series(1,10000) i;
-- Inserting nulls to verify results match when index key specifies nulls first or desc
INSERT INTO zoo values (null, null, null, null, null);
ANALYZE zoo;

-- Positive tests: Validate if IndexScan Forward/Backward is chosen.

-- single col index with default order
CREATE INDEX dir_index_a on zoo using btree(a);
-- Validate if 'dir_index_a' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward IndexScan
explain (costs off) select a from zoo order by a limit 3;
select a from zoo order by a limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select a from zoo order by a desc limit 3;
select a from zoo order by a desc limit 3;

-- single col index with reverse order
CREATE INDEX dir_index_b on zoo using btree(b desc);
-- Validate if 'dir_index_b' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward IndexScan
explain (costs off) select b from zoo order by b desc limit 3;
select b from zoo order by b desc limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select b from zoo order by b limit 3;
select b from zoo order by b limit 3;

-- single col index with opp nulls direction
CREATE INDEX dir_index_c on zoo using btree(c nulls first);
-- Validate if 'dir_index_c' is used for order by cols matching/commutative to the index cols
-- Expected to use Forward IndexScan
explain (costs off) select c from zoo order by c nulls first limit 3;
select c from zoo order by c nulls first limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select c from zoo order by c desc nulls last limit 3;
select c from zoo order by c desc nulls last limit 3;

-- multi col index all with all index keys asc
CREATE INDEX dir_index_bcd on zoo using btree(b,c,d);
-- Inserting rows with duplicate values to ensure results are sorted correctly for order by on multiple columns
INSERT INTO zoo(b,c) values('col_b1',-1);
INSERT INTO zoo(b,c) values('col_b9999',10000);
-- Validate if 'dir_index_bcd' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward IndexScan
explain (costs off) select b,c,d from zoo order by b,c,d limit 3;
select b,c,d from zoo order by b,c,d limit 3;
explain (costs off) select b,c from zoo order by b,c limit 3;
select b,c from zoo order by b,c limit 3;
-- Testing various permutations of order by columns that are expected to choose Backward IndexScan
explain (costs off) select b,c,d from zoo order by b desc,c desc,d desc limit 4;
select b,c,d from zoo order by b desc,c desc,d desc limit 4;
explain (costs off) select b,c from zoo order by b desc,c desc limit 4;
select b,c from zoo order by b desc,c desc limit 4;
-- Delete duplicate rows
delete from zoo where b='col_b1' and c=-1;
delete from zoo where b='col_b9999' and c=10000;

-- multi col index all with all index keys desc
CREATE INDEX dir_index_fde on zoo using btree(f desc,d desc,e desc);
-- Inserting rows with duplicate values to ensure results are sorted correctly for order by on multiple columns
INSERT INTO zoo(f,d) values(4,-1);
INSERT INTO zoo(f,d) values(10003,-1);
-- Validate if 'dir_index_fde' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward IndexScan
explain (costs off) select f,d,e from zoo order by f desc,d desc,e desc limit 4;
select f,d,e from zoo order by f desc,d desc,e desc limit 4;
explain (costs off) select f,d from zoo order by f desc,d desc limit 4;
select f,d from zoo order by f desc,d desc limit 4;
-- Testing various permutations of order by columns that are expected to choose Backward IndexScan
explain (costs off) select f,d,e from zoo order by f,d,e limit 3;
select f,d,e from zoo order by f,d,e limit 3;
explain (costs off) select f,d from zoo order by f,d limit 3;
select f,d from zoo order by f,d limit 3;
-- Delete duplicate rows
delete from zoo where f=4 and d=-1;
delete from zoo where f=10003 and d=-1;

-- multi col index with mixed index keys properties
CREATE INDEX dir_index_eda on zoo using btree(e, d desc nulls last,a);
-- Inserting rows with duplicate values to ensure results are sorted correctly for order by on multiple columns
INSERT INTO zoo(d,e) values(9999,'col_e9999');
INSERT INTO zoo(d,e) values(1,'col_e1');
-- Validate if 'dir_index_eda' is used for order by cols matching/commutative to the index cols
-- Testing various permutations of order by columns that are expected to choose Forward IndexScan
explain (costs off) select e,d,a from zoo order by e, d desc nulls last,a limit 3;
select e,d,a from zoo order by e, d desc nulls last,a limit 3;
explain (costs off) select e,d from zoo order by e, d desc nulls last limit 3;
select e,d from zoo order by e, d desc nulls last limit 3;
-- Testing various permutations of order by columns that are expected to choose Backward IndexScan
explain (costs off) select e,d,a from zoo order by e desc,d nulls first,a desc limit 4;
select e,d,a from zoo order by e desc,d nulls first,a desc limit 4;
explain (costs off) select e,d from zoo order by e desc,d nulls first limit 4;
select e,d from zoo order by e desc,d nulls first limit 4;
-- Delete duplicate rows
delete from zoo where d=9999 and e='col_e9999';
delete from zoo where d=1 and e='col_e1';

-- Covering index with descending and one include column
CREATE INDEX dir_covering_index_db ON zoo(d desc) INCLUDE (b);
-- Validate if IndexScan is chosen and on covering index
-- Expected to use Forward IndexScan
explain (costs off) select d from zoo order by d desc limit 3;
select d from zoo order by d desc limit 3;
-- Expected to use Backward IndexScan
explain (costs off) select d from zoo order by d limit 3;
select d from zoo order by d limit 3;

-- Validate if Backward IndexScan is chosen for query with offset and without limit
explain (costs off) select e,d,a from zoo order by e desc,d nulls first,a desc offset 9990;
select e,d,a from zoo order by e desc,d nulls first,a desc offset 9997;

-- Validate if Backward IndexScan is chosen for query with offset value in subquery
-- ORCA_FEATURE_NOT_SUPPORTED: ORCA doesn't support limit or offset values specified as part of a subquery
explain (costs off) select c from zoo order by c desc nulls last offset (select 9997);
select c from zoo order by c desc nulls last offset (select 9997);
-- Validate if Backward IndexScan is chosen for query with limit value in subquery
-- ORCA_FEATURE_NOT_SUPPORTED: ORCA doesn't support limit or offset values specified as part of a subquery
explain (costs off) select c from zoo order by c desc nulls last limit (select 3);
select c from zoo order by c desc nulls last limit (select 3);

-- Negative tests: Validate if a SeqScan is chosen if order by cols directions do not matching indices keys directions.
--                 Expected to choose SeqScan with Sort

-- Testing various permutations that are not matching keys in 'dir_index_a'
explain (costs off) select a from zoo order by a nulls first limit 3;
select a from zoo order by a nulls first limit 3;
explain (costs off) select a from zoo order by a desc nulls last limit 3;
select a from zoo order by a desc nulls last limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_b'
explain (costs off) select b from zoo order by b nulls first limit 3;
select b from zoo order by b nulls first limit 3;
explain (costs off) select b from zoo order by b desc nulls last limit 3;
select b from zoo order by b desc nulls last limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_c'
explain (costs off) select c from zoo order by c limit 3;
select c from zoo order by c  limit 3;
explain (costs off) select c from zoo order by c desc limit 3;
select c from zoo order by c desc limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_bcd'
explain (costs off) select b,c,d from zoo order by b ,c desc,d desc limit 3;
select b,c,d from zoo order by b ,c desc,d desc limit 3;
explain (costs off) select b,c,d from zoo order by b ,c ,d desc limit 3;
select b,c,d from zoo order by b ,c ,d desc limit 3;
explain (costs off) select b,c,d from zoo order by b desc, c ,d desc limit 3;
select b,c,d from zoo order by b desc, c ,d desc limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_fde'
explain (costs off) select f,d,e from zoo order by f ,d desc,e desc limit 3;
select f,d,e from zoo order by f ,d desc,e desc limit 3;
explain (costs off) select f,d,e from zoo order by f,d ,e desc limit 3;
select f,d,e from zoo order by f,d ,e desc limit 3;
explain (costs off) select f,d,e from zoo order by f desc, d ,e desc limit 3;
select f,d,e from zoo order by f desc, d ,e desc limit 3;

-- Testing various permutations that are not matching keys in 'dir_index_eda'
explain (costs off) select e,d,a from zoo order by e, d desc,a desc limit 3;
select e,d,a from zoo order by e, d desc,a desc limit 3;
explain (costs off) select e,d,a from zoo order by e desc,d desc,a desc limit 3;
select e,d,a from zoo order by e desc,d desc,a desc limit 3;
explain (costs off) select e,d,a from zoo order by e ,d ,a  limit 3;
select e,d,a from zoo order by e ,d ,a  limit 3;

-- Testing various permutations of order by on non-index columns. Expected to choose SeqScan with Sort
explain (costs off) select d, f from zoo order by d, f limit 3;
select d, f from zoo order by d, f limit 3;
explain (costs off) select a,e from zoo order by a,e limit 3;
select a,e from zoo order by a,e limit 3;
explain (costs off) select d,a from zoo order by d,a desc limit 3;
select d,a from zoo order by d,a desc limit 3;
explain (costs off) select d,c from zoo order by d desc,c limit 3;
select d,c from zoo order by d desc,c limit 3;

-- Validate if SeqScan is chosen if order by cols also have the Included Column of covering index
explain (costs off) select e,b from zoo order by e, b limit 3;
select e,b from zoo order by e,b limit 3;

-- Purpose: Validate if IndexOnlyScan Forward/Backward is chosen when required for queries with order by and limit
-- Vacuum table to Ensure IndexOnlyScan is chosen
vacuum zoo;
-- Testing various permutations of order by columns that are expected to choose IndexOnlyScan Forward
explain (costs off) select b from zoo order by b desc limit 3;
select b from zoo order by b desc limit 3;
explain (costs off) select e,d,a from zoo order by e, d desc nulls last limit 3;
select e,d,a from zoo order by e, d desc nulls last limit 3;
explain (costs off) select b,c,d from zoo order by b, c, d limit 3;
select b,c,d from zoo order by b, c, d limit 3;
explain (costs off) select f,d from zoo order by f desc,d desc,e desc limit 3;
select f,d from zoo order by f desc,d desc,e desc limit 3;
-- Testing various permutations of order by columns that are expected to choose IndexOnlyScan Backward
explain (costs off) select e,d,a from zoo order by e desc,d nulls first limit 3;
select e,d,a from zoo order by e desc,d nulls first limit 3;
explain (costs off) select e,d,a from zoo order by e desc,d nulls first,a desc limit 3;
select e,d,a from zoo order by e desc,d nulls first,a desc limit 3;
explain (costs off) select b,c from zoo order by b desc, c desc, d desc limit 3;
select b,c from zoo order by b desc, c desc, d desc limit 3;
explain (costs off) select f,d from zoo order by f, d limit 3;
select f,d from zoo order by f, d limit 3;

-- Clean Up
DROP TABLE zoo;


-- Test Case: Test on Leaf Partition of a partition table with mixed data type columns.
-- Purpose: Validate if IndexScan/IndexOnlyScan with correct scan direction is used on expected index for queries with order by and limit.

CREATE TABLE test_partition_table(a int, b int, c float, d text, e numeric, f int) DISTRIBUTED BY (a) PARTITION BY range(a);
CREATE TABLE partition1 PARTITION OF test_partition_table FOR VALUES FROM (1) TO (3000);
CREATE TABLE partition2 PARTITION OF test_partition_table FOR VALUES FROM (3000) TO (6000);
CREATE TABLE partition3 PARTITION OF test_partition_table FOR VALUES FROM (6000) TO (9000);
-- single col index with opp direction on partition column
CREATE INDEX part_index_ac on test_partition_table using btree(a desc, c);
INSERT INTO test_partition_table SELECT i, i+3, i/4.2, concat('sample_text ',i), i/5, i from generate_series(1,8998) i;
-- Inserting nulls to verify results match when index key specifies nulls first or desc
INSERT INTO test_partition_table values (8999, null, null, null, null, null);
ANALYZE test_partition_table;

-- Validate if IndexScan Forward/Backward are used for order by on partition2 table(as this is a regular table and don't
-- have further partitions)
explain(costs off) select a, c from partition2 order by a desc, c limit 3;
select a, c from partition2 order by a desc, c limit 3;
explain(costs off) select a, c from partition2 order by a, c desc limit 3;
select a, c from partition2 order by a, c desc limit 3;

-- Purpose: Validate if IndexOnlyScan Forward/Backward is chosen when required for queries with order by and limit
-- Vacuum table to ensure IndexOnlyScans are chosen
vacuum test_partition_table;
-- Expected to use IndexOnlyScan Forward
explain(costs off) select a, c from partition2 order by a desc, c limit 3;
select a, c from partition2 order by a desc, c limit 3;
-- Expected to use IndexOnlyScan Backward
explain(costs off) select a, c from partition2 order by a, c desc limit 3;
select a, c from partition2 order by a, c desc limit 3;

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
-- IndexOnlyScans are supported but IndexScans aren't supported on AO tables
CREATE TABLE test_ao_table(a int, b int, c float, d text, e numeric) WITH (appendonly=true) DISTRIBUTED BY (a);
-- multi col index with mixed index keys properties
CREATE INDEX ao_index_eda on test_ao_table using btree(e desc nulls last, d,a desc);
INSERT INTO test_ao_table SELECT i, i+3, i/4.2, concat('sample_text ',i), i/5 from generate_series(1,100) i;
-- Expected to choose IndexOnlyScan Forward
set enable_sort to off; -- help planner generate IndexOnlyScan
explain (costs off) select e,d,a from test_ao_table order by e desc nulls last, d, a desc limit 3;
select e,d,a from test_ao_table order by e desc nulls last, d, a desc limit 3;
-- Expected to choose IndexOnlyScan Backward
explain(costs off) select e,d,a from test_ao_table order by e nulls first, d desc, a limit 3;
select e,d,a from test_ao_table order by e nulls first, d desc, a limit 3;
reset enable_sort;

-- Expected to choose SeqScan with a Sort as IndexOnlyScan doesn't support, since it selects all columns
explain(costs off) select * from test_ao_table order by e desc nulls last, d, a desc limit 3;
select * from test_ao_table order by e desc nulls last, d, a desc limit 3;
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


-- Purpose: Test DynamicIndexScan on partition table with composite partition columns
-- ORCA_FEATURE_NOT_SUPPORTED: Composite partition keys are not supported in ORCA
CREATE TABLE tbl_range (id int, col1 int, col2 int, col3 int) PARTITION BY RANGE (col1, col2);
CREATE TABLE p1 PARTITION OF tbl_range FOR VALUES FROM (0, 0) TO (100, 100);
CREATE TABLE p2 PARTITION OF tbl_range FOR VALUES FROM (100, 100) TO (200, 200);
CREATE TABLE p3 PARTITION OF tbl_range FOR VALUES FROM (200, 200) TO (300, 300);
CREATE INDEX idx_on_tbl_range ON tbl_range using btree(col1, col2);
ANALYZE tbl_range;
-- Demonstrate that Planner could use IndexScan with MergeAppend for these cases
set enable_seqscan to off;
explain(costs off) select * from tbl_range order by col1, col2 limit 3;
explain(costs off) select * from tbl_range order by col1 limit 3;
-- Clean Up
DROP TABLE tbl_range;


-- Purpose: Test DynamicIndexScan on multi-level partition table
-- ORCA_FEATURE_NOT_SUPPORTED: Multi-level partitioned tables are not supported in ORCA
CREATE TABLE sales_data(year int, geo varchar(2), impressions integer, sales integer) PARTITION BY RANGE (year);
CREATE TABLE sales_data_18_20 PARTITION OF sales_data FOR VALUES FROM (2018) TO (2020) PARTITION BY LIST (geo);
CREATE TABLE sales_data_UK_18_20 PARTITION OF sales_data_18_20 FOR VALUES IN ('UK');
CREATE TABLE sales_data_US_18_20 PARTITION OF sales_data_18_20 FOR VALUES IN ('US');
CREATE INDEX idx_on_year ON sales_data USING btree(year desc);
ANALYZE sales_data;
explain(costs off) select * from sales_data order by year desc limit 3;
-- Clean Up
DROP TABLE sales_data;


-- Purpose: Test DynamicIndexScan on a hash partitioned table
-- ORCA_FEATURE_NOT_SUPPORTED: ORCA do not support hash partitioning
CREATE TABLE tbl_hash (id int, col1 int, col2 int, col3 int) PARTITION BY HASH (col1);
CREATE TABLE p1 PARTITION OF tbl_hash FOR VALUES WITH (MODULUS 100, REMAINDER 20);
CREATE TABLE p2 PARTITION OF tbl_hash FOR VALUES WITH (MODULUS 100, REMAINDER 30);
CREATE INDEX idx_on_tbl_hash ON tbl_hash using btree(col1);
ANALYZE tbl_hash;
explain(costs off) select * from tbl_hash order by col1 limit 3;
-- Clean Up
DROP TABLE tbl_hash;


-- Purpose: Test DynamicIndexScan on a list partitioned table
-- Currently, ORCA doesn't support DynamicIndex(Only)Scan on partition tables.
CREATE TABLE sales_data(year int, geo varchar(2), impressions integer, sales integer) PARTITION BY LIST (geo);
CREATE TABLE sales_data_UK PARTITION OF sales_data FOR VALUES IN ('UK');
CREATE TABLE sales_data_AU PARTITION OF sales_data FOR VALUES IN ('AU');
CREATE INDEX idx_on_geo ON sales_data using btree(geo);
ANALYZE sales_data;
-- Expected to use DynamicSeqScan with Sort
explain(costs off) select * from sales_data order by geo limit 3;
-- Clean Up
reset enable_seqscan;
DROP TABLE sales_data;


-- Purpose: Test DynamicIndexScan on partition table with holes and a default partition
-- Currently, ORCA doesn't support DynamicIndex(Only)Scan on partition tables.
CREATE TABLE tbl_range (id int, col1 int, col2 int, col3 int) PARTITION BY RANGE (col1);
CREATE TABLE p1 PARTITION OF tbl_range FOR VALUES FROM (100) TO (200);
CREATE TABLE p2 PARTITION OF tbl_range FOR VALUES FROM (200) TO (300);
CREATE TABLE p3 PARTITION OF tbl_range DEFAULT;
CREATE INDEX idx_on_tbl_range ON tbl_range using btree(col1);
INSERT INTO tbl_range select i-1,i,i*3,i/2 FROM generate_series(0, 400) i;
ANALYZE tbl_range;
-- Demonstrate that Planner could use IndexScan with MergeAppend for these cases
set enable_seqscan to off;
explain(costs off) select * from tbl_range order by col1 limit 3;
select * from tbl_range order by col1 limit 3;
-- Clean Up
DROP TABLE tbl_range;


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

-- NL Joins can utilize IndexScan's sort property, but currently ORCA doesn't
-- generate IndexScan alternatives for NL joins. This tests the case where
-- IndexScan's order property could be used for joining two tables
CREATE TABLE employee(id int, name text, dep_id int, salary int);
CREATE TABLE department(dep_id int, dep_name text);
CREATE INDEX index_salary on employee using btree(salary);
ANALYZE employee;
ANALYZE department;
-- Forcing planner, ORCA to use a NL join
set enable_hashjoin to off;
set optimizer_enable_hashjoin to off;
set enable_seqscan to off;
-- Planner uses NL Join with IndexScan Backwards and the sort property of index 'index_salary',
-- but ORCA, since it doesn't generate IndexScan alternative uses NL join with a Sort operator.
explain(costs off) select e.id, e.name, e.salary, d.dep_name from employee e join department d on e.id=d.dep_id order by e.salary desc;
explain(costs off) select e.id, e.name, e.salary, d.dep_name from employee e join department d on e.id=d.dep_id order by e.salary desc limit 3;
-- Clean up
reset enable_hashjoin;
reset optimizer_enable_hashjoin;
reset enable_seqscan;
DROP TABLE employee;
DROP TABLE department;


-- Union all of two tables with order by on their indexed column uses
-- IndexScan's sort property with MergeAppend node in Planner. However in ORCA
-- we don't generate IndexScan alternative for union all, also we don't support
-- MergeAppend. But documenting this case for reference
CREATE TABLE table1(a int, b int);
CREATE TABLE table2(a int, b int);
CREATE INDEX tab1_idx on table1 using btree(b);
CREATE INDEX tab2_idx on table2 using btree(b);
-- inserting data and disabling seq_scan to avoid Planner generating a plan with
-- Sort operator and Append node instead of IndexScan with MergeAppend
set enable_seqscan to off;
INSERT INTO table1 select i, i from generate_series(1,99)i;
INSERT INTO table2 select i, i from generate_series(1,99)i;
ANALYZE table1;
ANALYZE table2;
explain(costs off) select b from table1 union all select b from table2 order by b desc;
reset enable_seqscan;
DROP TABLE table1;
DROP TABLE table2;


-- Purpose: This section includes tests on partition table where backward
-- DynamicIndexScan could be used, but is not used currently since ORCA currently
-- doesn't support DynamicIndex(Only)Scan on partition tables.
CREATE TABLE test_partition_table(a int, b text, c float, d int, e int) DISTRIBUTED BY (a) PARTITION BY range(a) (start (0) end(10000) every(2000));
-- single col index on partition column
CREATE INDEX part_index_a on test_partition_table using btree(a);
-- multi col indices
CREATE INDEX part_index_cd on test_partition_table using btree(c,d);
CREATE INDEX part_index_bcd on test_partition_table using btree(b,c,d);
CREATE INDEX part_index_cad on test_partition_table using btree(c,a,d);

-- Inserting data to demonstrate that Planner chooses IndexScans for these cases
INSERT INTO test_partition_table select i, concat('sample_text', i), i/3.3, i,i-2 from generate_series(1,9999)i;
ANALYZE test_partition_table;

-- Currently ORCA doesn't support DynamicIndex(Only)Scans for order by on partition tables,
-- even if a btree index exists matching/commutative to order by cols. ORCA uses DynamicSeqScan with sort.
explain(costs off) select c,d from test_partition_table order by c, d  limit 3;

explain(costs off) select c,a,d from test_partition_table order by c,a,d limit 3;

explain(costs off) select b,c,d from test_partition_table order by b desc,c desc,d desc limit 3;

-- Cases with just order by without limit
explain(costs off) select a from test_partition_table order by a desc;

-- Since col a is asc in index, max(a) could use a backward index scan
explain(costs off) select max(a) from test_partition_table;

-- Cases with a predicate and order by (with/without limit). Order by columns commutating index column
explain(costs off) select * from test_partition_table where a BETWEEN 40 and 4000 or c not between 4000 and 6000 order by a desc;

explain(costs off) select * from test_partition_table where a>7 order by a desc limit 4;

-- Cases with group by, order by (with/without limit). Order by cols commutating index column
explain(costs off) select  a, sum(d) from test_partition_table group by a order by a desc;

explain(costs off) select  a, sum(d) from test_partition_table group by a order by a desc limit 3;

-- Case with group by, order by and a having clause (with/without limit). Order by cols commutating index.
explain(costs off) select  a, sum(d) from test_partition_table group by a having a>30 order by a desc;

explain(costs off) select  a, sum(d) from test_partition_table group by a having a>30 order by a desc;

-- Case with ordering via over() using window aggregates (with/without limit): rank(), row_number(), percent_rank() etc...
explain(costs off) select a, rank() over (order by a desc) from test_partition_table;

explain(costs off) select a, rank() over (order by a desc) from test_partition_table limit 3;

-- Case with distinct and order by (with/without limit)
explain(costs off) select distinct(a) from test_partition_table order by a desc;

explain(costs off) select distinct(a) from test_partition_table order by a desc limit 3;

-- Order by within a CTE without limit
explain(costs off) with sorted_by_a as (select a from test_partition_table order by a) select a from sorted_by_a;

-- Order by within a CTE, with limit outside CTE expression
explain(costs off) with sorted_by_a as (select a from test_partition_table order by a) select a from sorted_by_a limit 3;

-- Case where DynamicIndexOnlyScan Backwards could be picked, but ORCA fails to produce DynamicIndexOnlyScan alternative.
-- This is because, in FCoverIndex() function while determining output columns for the query we also consider partition
-- column as part of the output cols(though query doesn't project it) since partition colum is always marked as Used.
-- Due to this, for every index that doesn't include partition column as its key, index cols doesn't match
-- the output cols and hence alternative isn't added. This issue has to be fixed after adding support for MergeAppend.
vacuum test_partition_table;

explain(costs off) select b,c,d from test_partition_table order by b desc, c desc, d desc;

-- Clean Up
DROP TABLE test_partition_table;

-- NL Joins can utilize IndexScan's sort property, but currently ORCA doesn't
-- generate DynamicIndexScan alternatives for NL joins. This tests the case
-- where IndexScan's order property could be used for joining two partition
-- tables
CREATE TABLE part_employee(id int, name text, dep_id int, salary int) PARTITION BY range(id) (start (0) end(10000) every(2000));
CREATE TABLE part_department(dep_id int, dep_name text) PARTITION BY range(dep_id) (start (0) end(10000) every(2000));
CREATE INDEX part_index_id on part_employee using btree(id);
ANALYZE part_employee;
ANALYZE part_department;
-- Forcing planner, ORCA to use a NL join
set enable_hashjoin to off;
set optimizer_enable_hashjoin to off;
set enable_seqscan to off;
-- Planner uses NL Join with IndexScan Backwards and the sort property of index
-- 'index_salary', but ORCA since it doesn't generate DynamicIndexScan
-- alternative uses NL join with a Sort operator.
explain(costs off) select e.id, e.name, e.salary, d.dep_name from part_employee e join part_department d on e.id=d.dep_id order by e.id desc;
explain(costs off) select e.id, e.name, e.salary, d.dep_name from part_employee e join part_department d on e.id=d.dep_id order by e.id desc limit 3;
-- Clean up
reset enable_hashjoin;
reset optimizer_enable_hashjoin;
reset enable_seqscan;
DROP TABLE part_employee;
DROP TABLE part_department;

-- Union all of two partition tables with order by on their indexed column uses
-- IndexScan's sort property with MergeAppend node in Planner. However in ORCA
-- we don't generate DynamicIndexScan alternative for union all, also we don't
-- support MergeAppend. But documenting this case for reference
CREATE TABLE part_table1(a int, b int) PARTITION BY range(a) (start (0) end(100) every(20));
CREATE TABLE part_table2(a int, b int) PARTITION BY range(a) (start (0) end(100) every(20));
CREATE INDEX part_tab1_idx on part_table1 using btree(a);
CREATE INDEX part_tab2_idx on part_table2 using btree(a);
-- inserting data and disabling seq_scan to avoid Planner generating a plan with
-- Sort operator and Append node instead of IndexScan with MergeAppend
set enable_seqscan to off;
INSERT INTO part_table1 select i, i from generate_series(1,99)i;
INSERT INTO part_table2 select i, i from generate_series(1,99)i;
ANALYZE part_table1;
ANALYZE part_table2;
explain(costs off) select a from part_table1 union all select a from part_table2 order by a desc;
-- Clean Up
reset enable_seqscan;
DROP TABLE part_table1;
DROP TABLE part_table2;

-- Purpose: This section includes tests related to min(), max() aggregates optimization.
CREATE TABLE min_max_aggregates(a int, b int, c int);
CREATE INDEX multi_key_index_b_c on min_max_aggregates using btree(b DESC, c);
ANALYZE min_max_aggregates;

-- Testing min/max functions when table doesn't have any tuples to
-- ensure they return 1 NULL row indicating no min or max value exists.
-- This test is eligible for optimization.
explain(costs off) select min(b) from min_max_aggregates;
select min(b) from min_max_aggregates;
explain(costs off) select max(b) from min_max_aggregates;
select max(b) from min_max_aggregates;

INSERT INTO min_max_aggregates select i, i, i from generate_series(1,100)i;
INSERT INTO min_max_aggregates values(null, null, null);

-- Creating this index to show that both 'multi_key_index_b_c' and
-- 'single_key_b_desc' are eligible for min/max on column 'b' and
-- ORCA picks the lower cost index
CREATE INDEX single_key_b_desc on min_max_aggregates using btree(b DESC);
ANALYZE min_max_aggregates;

-- This query is eligible for optimization and it leverages Backward
-- IndexScan as column 'b' in 'single_key_b_desc' is sorted in
-- descending order and therefore, minimum value can be found at
-- the bottom of the index
explain(costs off) select min(b) from min_max_aggregates;
select min(b) from min_max_aggregates;
-- This query is eligible for optimization and it leverages Forward
-- IndexScan as column 'b' in 'single_key_b_desc' is sorted in
-- descending order and therefore, maximum value can be found
-- at the top of the index
explain(costs off) select max(b) from min_max_aggregates;
select max(b) from min_max_aggregates;

DROP INDEX single_key_b_desc;

-- Test min/max optimization behavior on index with key direction ASC
CREATE INDEX single_key_b_asc on min_max_aggregates using btree(b);
ANALYZE min_max_aggregates;

-- This query is eligible for optimization and it leverages Forward
-- IndexScan as column 'b' in 'single_key_b_asc' is sorted in
-- ascending order and therefore, minimum value can be found at the
-- top of the index
explain(costs off) select min(b) from min_max_aggregates;
select min(b) from min_max_aggregates;
-- This query is eligible for optimization and it leverages Backward
-- IndexScan as column 'b' in 'single_key_b_asc' is sorted in
-- ascending order and therefore, maximum value can be found
-- at the bottom of the index
explain(costs off) select max(b) from min_max_aggregates;
select max(b) from min_max_aggregates;

DROP INDEX single_key_b_asc;

-- This query is not eligible for optimization as min/max aggregates
-- are applied on non-leading keys in the index.
explain(costs off) select min(c) from min_max_aggregates;
explain(costs off) select max(c) from min_max_aggregates;

-- This query is not eligible for optimization as min/max aggregates
-- are applied on non index column.
explain(costs off) select min(a) from min_max_aggregates;
explain(costs off) select max(a) from min_max_aggregates;

-- Test min/max on a constant. This query is not eligible for
-- optimization as it is not necessary for min/max on constants
explain(costs off) select min(100) from min_max_aggregates;
explain(costs off) select max(100) from min_max_aggregates;

-- Test min/max optimization behavior with empty group by. This query
-- is eligible for optimization as it doesn't specify any grouping columns
explain(costs off) select min(b) from min_max_aggregates group by ();
select min(b) from min_max_aggregates group by ();
explain(costs off) select max(b) from min_max_aggregates group by ();
select max(b) from min_max_aggregates group by ();

-- Test min/max with non-empty group by. This query is not eligible for
-- optimization as it has grouping columns. This check is made while
-- computing xform promise by validating size of grouping columns and
-- there by avoiding application of transform
explain(costs off) select min(b) from min_max_aggregates group by b;
explain(costs off) select max(b) from min_max_aggregates group by b;

-- Test min/max optimization with CTE

-- Test min/max optimization when used in CTE producer. This query is
-- eligible for optimization as producer computes min aggregate on a
-- btree index key
explain (costs off) with cte_producer as (select min(b) as min_b from min_max_aggregates) select min_b from cte_producer;
with cte_producer as (select min(b) as min_b from min_max_aggregates) select min_b from cte_producer;

-- Test min/max optimization when used in CTE consumer. This query is
-- eligible for optimization as CTE consumer computes min aggregate on a
-- btree index key projected by CTE producer
explain (costs off) with cte_consumer as (select b as col_b from min_max_aggregates) select min(col_b) from cte_consumer;
with cte_consumer as (select b as col_b from min_max_aggregates) select min(col_b) from cte_consumer;

-- Test min/max optimization when used in CTE consumer. This query is not
-- eligible for optimization, because the subquery projects 'col_b' as 'b/2'
-- upon which the min is computed, but none of the indices store values
-- of column 'b/2' so that IndexScan could be used on that index
explain (costs off) with cte_consumer as (select b/2 as col_b from min_max_aggregates) select min(col_b) from cte_consumer;

-- Test min/max optimization with Casts

-- Case where result of min/max is casted to a different data type.
-- This query is eligible for optimization as casting happens after
-- aggregation.
explain (costs off) select min(b)::int8 from min_max_aggregates;
select min(b)::int8 from min_max_aggregates;

-- Case where min/max is computed on a column casted to a different data type.
-- This query is eligible for optimization as column type int4 and casted
-- type int8 belong to same opfamily
explain (costs off) select min(b::int8) from min_max_aggregates;
select min(b::int8) from min_max_aggregates;

-- This query is not eligible for optimization as there is no operator
-- that handles comparison of a int4 and varchar type
explain (costs off) select min(b::varchar) from min_max_aggregates;

-- Cases with more than one min/max aggregates in query

-- These queries aren't eligible for optimization because the transform's
-- pattern only contains a single Scalar Project Element that matches only
-- a single aggregate function whereas, these queries have more than one
-- aggregate function. Support for these queries is beyond the scope of
-- the current PR
explain(costs off) select min(b), max(b) from min_max_aggregates;
explain(costs off) select min(a) + max(a) from min_max_aggregates;

-- Clean Up
drop table min_max_aggregates;

-- Test min/max optimization with union all, subqueries, joins and outer references
CREATE TABLE table1 (a int, b int, c int);
CREATE INDEX t1_c_idx on table1 using btree(c);
CREATE TABLE table2 (a int, b int, c int);
CREATE INDEX t2_c_idx on table2 using btree(c);
INSERT INTO table1 select i, i, i from generate_series(1,100) i;
INSERT INTO table2 select i, i, i from generate_series(1,100) i;
ANALYZE table1;
ANALYZE table2;

-- Test min/max optimization when used in subqueries

-- This query is eligible for optimization as subquery computes max
-- aggregate on a btree index key
explain (costs off) select b from table1 where c = (select max(c) from table1);
select b from table1 where c = (select max(c) from table1);

-- Test min/max optimization in a subquery along with a predicate.
-- This query isn't eligible to use optimization because predicate's
-- pattern has a Select over LogicalGet and it doesn't match with the
-- transform's pattern
explain (costs off) select b, (select min(c) from table1 where table1.a > 5) as min_c from table2;

explain (costs off) select b, (select min(c) from table1 where table1.b = table2.b) as min_c from table2;

-- Test min/max optimization on result of union all present as part of
-- subquery. This query is not eligible to use min/max optimization as
-- it performs min/max on result of union all, and not directly on a table's
-- column due to which there is mismatch in query and transform's pattern.
-- The query pattern has LogicalUnionAll as first child of GbAgg whereas
-- transforms pattern has LogicalGet.
explain (costs off) select max(c) from (select c from table1 union all select c from table2) subquery;

-- Test min/max optimization on outer references.

-- This query is eligible to use optimization as it only has single
-- aggregate function on an index column
explain (costs off) select (select b from table1 t1_alias where t1_alias.a = min(t1.c)) as min_val_for_c from table1 t1;

-- This query uses more than one aggregate function, and is not eligible
-- to use optimization. This is because, query's pattern doesn't
-- match the transforms pattern of a single Scalar Project Element.
explain (costs off) select min(t1.c) as min_c,
                            (select b from table1 t1_alias where t1_alias.c = max(t1.c)) as b_val
                    from table1 t1;

-- Test min/max optimization used as part of projected columns in join.
-- This query is not eligible to use the optimization as it involves
-- join result which is not guaranteed to be sorted, unless it is an NL join.
explain (costs off) select min(table1.c) from table1 join table2 on table1.a=table2.a;

-- Clean up
drop table table1;
drop table table2;

-- Purpose: This section tests IS NULL/IS NOT NULL predicate on btree and non-index columns
CREATE TABLE test_nulltype_predicates(a int, b int);
CREATE INDEX index_b on test_nulltype_predicates using btree(b DESC);
INSERT INTO test_nulltype_predicates select i, i from generate_series(1,3)i;
INSERT INTO test_nulltype_predicates values(null, null);
ANALYZE test_nulltype_predicates;

-- Tests with IS NULL on btree index columns
explain(costs off) select * from test_nulltype_predicates where b is null;
select * from test_nulltype_predicates where b is null;

-- Tests with IS NULL on non-index columns
explain(costs off) select * from test_nulltype_predicates where a is null;

-- Tests with IS NOT NULL on btree index columns
explain(costs off) select * from test_nulltype_predicates where b is not null;
select * from test_nulltype_predicates where b is not null;

-- Tests with IS NOT NULL on non-index columns
explain(costs off) select * from test_nulltype_predicates where a is not null;

-- Clean Up
drop table test_nulltype_predicates;

-- Purpose: Test min/max optimization on AO table.
-- IndexOnlyScans are supported but IndexScans aren't supported on AO tables
CREATE TABLE test_ao_table(a int, b int) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX ao_index_b on test_ao_table using btree(b desc);
INSERT INTO test_ao_table SELECT i, i from generate_series(1,100) i;
ANALYZE test_ao_table;

-- Test max() aggregate. This query is eligible to use optimization
explain(costs off) select max(b) from test_ao_table;
select max(b) from test_ao_table;

-- Test min() aggregate. This query is eligible to use optimization
explain(costs off) select min(b) from test_ao_table;
select min(b) from test_ao_table;

-- Clean Up
drop table test_ao_table;


-- Purpose: Test min/max optimization on partition tables.
CREATE TABLE test_partition_table(a int, b int) DISTRIBUTED BY (a) PARTITION BY range(b);
CREATE TABLE partition1 PARTITION OF test_partition_table FOR VALUES FROM (1) TO (3);
CREATE TABLE partition2 PARTITION OF test_partition_table FOR VALUES FROM (3) TO (6);
CREATE TABLE default_partition PARTITION OF test_partition_table DEFAULT;
CREATE INDEX part_index_b on test_partition_table using btree(b desc);
INSERT INTO test_partition_table SELECT i, i from generate_series(1,4) i;
-- Insert into default partition to show partition pruning
-- for IS NULL conditions
INSERT INTO test_partition_table values (0, NULL);
ANALYZE test_partition_table;

-- Test min/max aggregate on partition tables. This query is not
-- eligible for optimization because the query's pattern has LogicalDynamicGet
-- as first child of LogicalGbAgg whereas transform's pattern has LogicalGet.
-- Support for these queries is beyond the scope of the current PR.
explain(costs off) select max(b) from test_partition_table;

explain(costs off) select min(b) from test_partition_table;

-- Test IS NULL, IS NOT NULL on partition table btree index column.
-- For IS NULL predicate on partition column, pruning happens
-- whereas, for IS NOT NULL it doesn't because the Non null values
-- could be in all of the partitions
explain(costs off) select * from test_partition_table where b is null;
select * from test_partition_table where b is null;
explain(costs off) select * from test_partition_table where b is not null;
select * from test_partition_table where b is not null;

-- Clean Up
drop table test_partition_table;
