-- tests MPP-2614
-- two segments no mirrors
drop table if exists hashagg_test;
create table hashagg_test (id1 int4, id2 int4, day date, grp text, v int4);
create index hashagg_test_idx on hashagg_test (id1,id2,day,grp);
insert into hashagg_test values (1,1,'1/1/2006','there',1);
insert into hashagg_test values (1,1,'1/2/2006','there',2);
insert into hashagg_test values (1,1,'1/3/2006','there',3);
insert into hashagg_test values (1,1,'1/1/2006','hi',2);
insert into hashagg_test values (1,1,'1/2/2006','hi',3);
insert into hashagg_test values (1,1,'1/3/2006','hi',4);

-- this will get the wrong answer (right number of rows, wrong aggregates)
set enable_seqscan=off;
select grp,sum(v) from hashagg_test where id1 = 1 and id2 = 1 and day between '1/1/2006' and '1/31/2006' group by grp order by sum(v) desc;

create index hashagg_test_idx_bm on hashagg_test using bitmap (id1,id2,day,grp);
set enable_indexscan=off; -- turn off b-tree index
select grp,sum(v) from hashagg_test where id1 = 1 and id2 = 1 and day between '1/1/2006' and '1/31/2006' group by grp order by sum(v) desc;

-- correct answer
set enable_seqscan=on;
select grp,sum(v) from hashagg_test where id1 = 1 and id2 = 1 and day between '1/1/2006' and '1/31/2006' group by grp order by sum(v) desc;


-- Test a window-aggregate (median) with a join where the join column
-- is further constrained by a constant. And the constant is of different
-- type (int4, while the column is bigint). We had a bug at one point
-- where this threw an error.

create table tbl_a (id bigint) distributed by (id);
create table tbl_b (id bigint, t numeric) distributed by (id);

insert into tbl_a values (1), (2), (3);
insert into tbl_b values (1, 50), (1, 100), (1, 150), (2, 200), (4, 400);

select tbl_a.id, median (t) from tbl_a, tbl_b
where tbl_a.id = tbl_b.id and tbl_a.id = 1::int4
group by tbl_a.id ;

--
-- Test that the planner doesn't try to use a hash agg for a type that is not hashable.
--

-- First create a type to test with.
CREATE TYPE nohash_int;
CREATE FUNCTION nohash_int_in(cstring) RETURNS nohash_int IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4in';
CREATE FUNCTION nohash_int_out(nohash_int) RETURNS cstring IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4out';
CREATE TYPE nohash_int (INPUT = nohash_int_in, OUTPUT=nohash_int_out, INTERNALLENGTH=4, PASSEDBYVALUE, ALIGNMENT=int4);


-- Create comparison operators and opclass.
CREATE FUNCTION nohash_int_cmp(nohash_int, nohash_int) RETURNS integer AS 'btint4cmp' LANGUAGE 'internal' IMMUTABLE STRICT;
CREATE FUNCTION nohash_int_lt(nohash_int, nohash_int) RETURNS bool IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4lt';
CREATE FUNCTION nohash_int_le(nohash_int, nohash_int) RETURNS bool IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4le';
CREATE FUNCTION nohash_int_eq(nohash_int, nohash_int) RETURNS bool IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4eq';
CREATE FUNCTION nohash_int_ge(nohash_int, nohash_int) RETURNS bool IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4ge';
CREATE FUNCTION nohash_int_gt(nohash_int, nohash_int) RETURNS bool IMMUTABLE STRICT LANGUAGE INTERNAL AS 'int4gt';

CREATE OPERATOR <  (PROCEDURE=nohash_int_lt, LEFTARG=nohash_int, RIGHTARG=nohash_int);
CREATE OPERATOR <= (PROCEDURE=nohash_int_le, LEFTARG=nohash_int, RIGHTARG=nohash_int);
CREATE OPERATOR =  (PROCEDURE=nohash_int_eq, LEFTARG=nohash_int, RIGHTARG=nohash_int);
CREATE OPERATOR >= (PROCEDURE=nohash_int_ge, LEFTARG=nohash_int, RIGHTARG=nohash_int);
CREATE OPERATOR >  (PROCEDURE=nohash_int_gt, LEFTARG=nohash_int, RIGHTARG=nohash_int);

CREATE OPERATOR CLASS nohash_int_bt_ops DEFAULT
	FOR TYPE nohash_int USING btree AS
	OPERATOR 1  <,
	OPERATOR 2  <=,
	OPERATOR 3  =,
	OPERATOR 4  >=,
	OPERATOR 5  >,
	FUNCTION 1  nohash_int_cmp(nohash_int, nohash_int);


-- A test table
create table hashagg_test2(normal_int int4, nohash_int nohash_int) distributed randomly;
insert into hashagg_test2 select g%10, (g%10)::text::nohash_int from generate_series(1, 10000) g;

-- Prefer hash aggs
set enable_sort=off;

-- These should both work. The first is expected to use a hash agg. The second will
-- use a Sort + Group, because nohash_int type is not hashable.
select normal_int from hashagg_test2 group by normal_int;
select nohash_int from hashagg_test2 group by nohash_int;

reset enable_sort;

-- We had a bug in the following combine functions where if the combine function
-- returned a NULL, it didn't set fcinfo->isnull = true. This led to a segfault
-- when we would spill in the final stage of a two-stage agg inside the serial
-- function.
CREATE TABLE test_combinefn_null (a int8, b int, c char(32000));
INSERT INTO test_combinefn_null SELECT i, (i | 15), i::text FROM generate_series(1, 1024) i;
ANALYZE test_combinefn_null;
SET statement_mem='2MB';
set enable_sort=off;

-- Test int8_avg_combine()
SELECT $$
SELECT
sum(a) FILTER (WHERE false)
FROM test_combinefn_null
GROUP BY b
HAVING max(c) = '31'
$$ AS qry \gset
EXPLAIN (COSTS OFF, VERBOSE) :qry;
:qry;

-- Test numeric_poly_combine()
SELECT $$
SELECT
var_pop(a::int) FILTER (WHERE false)
FROM test_combinefn_null
GROUP BY b
HAVING max(c) = '31'
$$ AS qry \gset
EXPLAIN (COSTS OFF, VERBOSE) :qry;
:qry;

-- Test numeric_avg_combine()
SELECT $$
SELECT
sum(a::numeric) FILTER (WHERE false)
FROM test_combinefn_null
GROUP BY b
HAVING max(c) = '31'
$$ AS qry \gset
EXPLAIN (COSTS OFF, VERBOSE) :qry;
:qry;

-- Test numeric_combine()
SELECT $$
SELECT
var_pop(a::numeric) FILTER (WHERE false)
FROM test_combinefn_null
GROUP BY b
HAVING max(c) = '31'
$$ AS qry \gset
EXPLAIN (COSTS OFF, VERBOSE) :qry;
:qry;
