
-- test numeric hash join

set gp_recursive_cte_prototype = true;

set enable_hashjoin to on;
set enable_mergejoin to off;
set enable_nestloop to off;
create table nhtest (i numeric(10, 2)) distributed by (i);
insert into nhtest values(100000.22);
insert into nhtest values(300000.19);
explain select * from nhtest a join nhtest b using (i);
select * from nhtest a join nhtest b using (i);

create temp table l(a int);
insert into l values (1), (1), (2);
select * from l l1 join l l2 on l1.a = l2.a left join l l3 on l1.a = l3.a and l1.a = 2 order by 1,2,3;

--
-- test hash join
--

create table hjtest (i int, j int) distributed by (i,j);
insert into hjtest values(3, 4);

select count(*) from hjtest a1, hjtest a2 where a2.i = least (a1.i,4) and a2.j = 4;

--
-- Test for correct behavior when there is a Merge Join on top of Materialize
-- on top of a Motion :
-- 1. Use FULL OUTER JOIN to induce a Merge Join
-- 2. Use a large tuple size to induce a Materialize
-- 3. Use gp_dist_random() to induce a Redistribute
--

set enable_hashjoin to off;
set enable_mergejoin to on;
set enable_nestloop to off;

DROP TABLE IF EXISTS alpha;
DROP TABLE IF EXISTS theta;

CREATE TABLE alpha (i int, j int) distributed by (i);
CREATE TABLE theta (i int, j char(10000000)) distributed by (i);

INSERT INTO alpha values (1, 1), (2, 2);
INSERT INTO theta values (1, 'f'), (2, 'g');

SELECT *
FROM gp_dist_random('alpha') FULL OUTER JOIN gp_dist_random('theta')
  ON (alpha.i = theta.i)
WHERE (alpha.j IS NULL or theta.j IS NULL);

reset enable_hashjoin;
reset enable_mergejoin;
reset enable_nestloop;

--
-- Predicate propagation over equality conditions
--

drop schema if exists pred;
create schema pred;
set search_path=pred;

create table t1 (x int, y int, z int) distributed by (y);
create table t2 (x int, y int, z int) distributed by (x);
insert into t1 select i, i, i from generate_series(1,100) i;
insert into t2 select * from t1;

analyze t1;
analyze t2;

--
-- infer over equalities
--
explain select count(*) from t1,t2 where t1.x = 100 and t1.x = t2.x;
select count(*) from t1,t2 where t1.x = 100 and t1.x = t2.x;

--
-- infer over >=
--
explain select * from t1,t2 where t1.x = 100 and t2.x >= t1.x;
select * from t1,t2 where t1.x = 100 and t2.x >= t1.x;

--
-- multiple inferences
--

set optimizer_segments=2;
explain select * from t1,t2 where t1.x = 100 and t1.x = t2.y and t1.x <= t2.x;
reset optimizer_segments;
select * from t1,t2 where t1.x = 100 and t1.x = t2.y and t1.x <= t2.x;

--
-- MPP-18537: hash clause references a constant in outer child target list
--

create table hjn_test (i int, j int) distributed by (i,j);
insert into hjn_test values(3, 4);
create table int4_tbl (f1 int) distributed by (f1);
insert into int4_tbl values(123456), (-2147483647), (0), (-123456), (2147483647);
select count(*) from hjn_test, (select 3 as bar) foo where hjn_test.i = least (foo.bar,4) and hjn_test.j = 4;
select count(*) from hjn_test, (select 3 as bar) foo where hjn_test.i = least (foo.bar,(array[4])[1]) and hjn_test.j = (array[4])[1];
select count(*) from hjn_test, (select 3 as bar) foo where least (foo.bar,(array[4])[1]) = hjn_test.i and hjn_test.j = (array[4])[1];
select count(*) from hjn_test, (select 3 as bar) foo where hjn_test.i = least (foo.bar, least(4,10)) and hjn_test.j = least(4,10);
select * from int4_tbl a join int4_tbl b on (a.f1 = (select f1 from int4_tbl c where c.f1=b.f1));

-- Same as the last query, but with a partitioned table (which requires a
-- Result node to do projection of the hash expression, as Append is not
-- projection-capable)
create table part4_tbl (f1 int4) partition by range (f1) (start(-1000000) end (1000000) every (1000000));
insert into part4_tbl values
       (-123457), (-123456), (-123455),
       (-1), (0), (1),
       (123455), (123456), (123457);
select * from part4_tbl a join part4_tbl b on (a.f1 = (select f1 from int4_tbl c where c.f1=b.f1));

--
-- Test case where a Motion hash key is only needed for the redistribution,
-- and not returned in the final result set. There was a bug at one point where
-- tjoin.c1 was used as the hash key in a Motion node, but it was not added
-- to the sub-plans target list, causing a "variable not found in subplan
-- target list" error.
--
create table tjoin1(dk integer, id integer) distributed by (dk);
create table tjoin2(dk integer, id integer, t text) distributed by (dk);
create table tjoin3(dk integer, id integer, t text) distributed by (dk);

insert into tjoin1 values (1, 1), (1, 2), (1, 3), (2, 1), (2, 2), (2, 3);
insert into tjoin2 values (1, 1, '1-1'), (1, 2, '1-2'), (2, 1, '2-1'), (2, 2, '2-2');
insert into tjoin3 values (1, 1, '1-1'), (2, 1, '2-1');

select tjoin1.id, tjoin2.t, tjoin3.t
from tjoin1
left outer join (tjoin2 left outer join tjoin3 on tjoin2.id=tjoin3.id) on tjoin1.id=tjoin3.id;


set enable_hashjoin to off;
set optimizer_enable_hashjoin = off;

select count(*) from hjn_test, (select 3 as bar) foo where hjn_test.i = least (foo.bar,4) and hjn_test.j = 4;
select count(*) from hjn_test, (select 3 as bar) foo where hjn_test.i = least (foo.bar,(array[4])[1]) and hjn_test.j = (array[4])[1];
select count(*) from hjn_test, (select 3 as bar) foo where least (foo.bar,(array[4])[1]) = hjn_test.i and hjn_test.j = (array[4])[1];
select count(*) from hjn_test, (select 3 as bar) foo where hjn_test.i = least (foo.bar, least(4,10)) and hjn_test.j = least(4,10);
select * from int4_tbl a join int4_tbl b on (a.f1 = (select f1 from int4_tbl c where c.f1=b.f1));

reset enable_hashjoin;
reset optimizer_enable_hashjoin;

-- In case of Left Anti Semi Join, if the left rel is empty a dummy join
-- should be created
drop table if exists foo;
drop table if exists bar;
create table foo (a int, b int) distributed randomly;
create table bar (c int, d int) distributed randomly;
insert into foo select generate_series(1,10);
insert into bar select generate_series(1,10);

explain select a from foo where a<1 and a>1 and not exists (select c from bar where c=a);
select a from foo where a<1 and a>1 and not exists (select c from bar where c=a);

-- The merge join executor code doesn't support LASJ_NOTIN joins. Make sure
-- the planner doesn't create an invalid plan with them.
create index index_foo on foo (a);
create index index_bar on bar (c);

set enable_nestloop to off;
set enable_hashjoin to off;
set enable_mergejoin to on;

select * from foo where a not in (select c from bar where c <= 5);

set enable_nestloop to off;
set enable_hashjoin to on;
set enable_mergejoin to off;

create table dept
(
	id int,
	pid int,
	name char(40)
);

insert into dept values(3, 0, 'root');
insert into dept values(4, 3, '2<-1');
insert into dept values(5, 4, '3<-2<-1');
insert into dept values(6, 4, '4<-2<-1');
insert into dept values(7, 3, '5<-1');
insert into dept values(8, 7, '5<-1');
insert into dept select i, i % 6 + 3 from generate_series(9,50) as i;
insert into dept select i, 99 from generate_series(100,15000) as i;

ANALYZE dept;

-- Test rescannable hashjoin with spilling hashtable for buffile
set statement_mem='1000kB';
set gp_workfile_type_hashjoin=buffile;
WITH RECURSIVE subdept(id, parent_department, name) AS
(
	-- non recursive term
	SELECT * FROM dept WHERE name = 'root'
	UNION ALL
	-- recursive term
	SELECT d.* FROM dept AS d, subdept AS sd
		WHERE d.pid = sd.id
)
SELECT count(*) FROM subdept;

-- Test rescannable hashjoin with spilling hashtable for bfz
set gp_workfile_type_hashjoin=bfz;
WITH RECURSIVE subdept(id, parent_department, name) AS
(
	-- non recursive term
	SELECT * FROM dept WHERE name = 'root'
	UNION ALL
	-- recursive term
	SELECT d.* FROM dept AS d, subdept AS sd
		WHERE d.pid = sd.id
)
SELECT count(*) FROM subdept;

-- Test rescannable hashjoin with in-memory hashtable
reset statement_mem;
WITH RECURSIVE subdept(id, parent_department, name) AS
(
	-- non recursive term
	SELECT * FROM dept WHERE name = 'root'
	UNION ALL
	-- recursive term
	SELECT d.* FROM dept AS d, subdept AS sd
		WHERE d.pid = sd.id
)
SELECT count(*) FROM subdept;

-- MPP-29458
-- When we join on a clause with two different types. If one table distribute by one type, the query plan
-- will redistribute data on another type. But the has values of two types would not be equal. The data will
-- redistribute to wrong segments.
create table test_timestamp_t1 (id  numeric(10,0) ,field_dt date) distributed by (id);
create table test_timestamp_t2 (id numeric(10,0),field_tms timestamp without time zone) distributed by (id,field_tms);

insert into test_timestamp_t1 values(10 ,'2018-1-10');
insert into test_timestamp_t1 values(11 ,'2018-1-11');
insert into test_timestamp_t2 values(10 ,'2018-1-10'::timestamp);
insert into test_timestamp_t2 values(11 ,'2018-1-11'::timestamp);

-- Test nest loop redistribute keys
set enable_nestloop to on;
set enable_hashjoin to on;
set enable_mergejoin to on;
select count(*) from test_timestamp_t1 t1 ,test_timestamp_t2 t2 where T1.id = T2.id and T1.field_dt = t2.field_tms;

-- Test hash join redistribute keys
set enable_nestloop to off;
set enable_hashjoin to on;
set enable_mergejoin to on;
select count(*) from test_timestamp_t1 t1 ,test_timestamp_t2 t2 where T1.id = T2.id and T1.field_dt = t2.field_tms;

drop table test_timestamp_t1;
drop table test_timestamp_t2;

-- Test merge join redistribute keys
create table test_timestamp_t1 (id  numeric(10,0) ,field_dt date) distributed randomly;

create table test_timestamp_t2 (id numeric(10,0),field_tms timestamp without time zone) distributed by (field_tms);

insert into test_timestamp_t1 values(10 ,'2018-1-10');
insert into test_timestamp_t1 values(11 ,'2018-1-11');
insert into test_timestamp_t2 values(10 ,'2018-1-10'::timestamp);
insert into test_timestamp_t2 values(11 ,'2018-1-11'::timestamp);

select * from test_timestamp_t1 t1 full outer join test_timestamp_t2 t2 on T1.id = T2.id and T1.field_dt = t2.field_tms;

-- test float type
set enable_nestloop to off;
set enable_hashjoin to on;
set enable_mergejoin to on;
create table test_float1(id int, data float4)  DISTRIBUTED BY (data);
create table test_float2(id int, data float8)  DISTRIBUTED BY (data);
insert into test_float1 values(1, 10), (2, 20);
insert into test_float2 values(3, 10), (4, 20);
select t1.id, t1.data, t2.id, t2.data from test_float1 t1, test_float2 t2 where t1.data = t2.data;

-- test int type
create table test_int1(id int, data int4)  DISTRIBUTED BY (data);
create table test_int2(id int, data int8)  DISTRIBUTED BY (data);
insert into test_int1 values(1, 10), (2, 20);
insert into test_int2 values(3, 10), (4, 20);
select t1.id, t1.data, t2.id, t2.data from test_int1 t1, test_int2 t2 where t1.data = t2.data;

-- Cleanup
reset enable_hashjoin;
set client_min_messages='warning'; -- silence drop-cascade NOTICEs
drop schema pred cascade;
reset search_path;

-- tests to ensure that join reordering of LOJs and inner joins produces the
-- correct join predicates & residual filters
reset all;
drop table if exists t1, t2, t3;
CREATE TABLE t1 (a int, b int, c int);
CREATE TABLE t2 (a int, b int, c int);
CREATE TABLE t3 (a int, b int, c int);
INSERT INTO t1 SELECT i, i, i FROM generate_series(1, 1000) i;
INSERT INTO t2 SELECT i, i, i FROM generate_series(2, 1000) i; -- start from 2 so that one row from t1 doesn't match
INSERT INTO t3 VALUES (1, 2, 3), (NULL, 2, 2);
ANALYZE t1;
ANALYZE t2;
ANALYZE t3;

-- ensure plan has a filter over left outer join
explain select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a IS NULL OR (t1.c = t3.c));
select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a IS NULL OR (t1.c = t3.c));

-- ensure plan has two inner joins with the where clause & join predicates ANDed
explain select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a = t3.a);
select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a = t3.a);

-- ensure plan has a filter over left outer join
explain select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a is distinct from t3.a);
select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a is distinct from t3.a);

-- ensure plan has a filter over left outer join
explain select * from t3 join (select t1.a t1a, t1.b t1b, t1.c t1c, t2.a t2a, t2.b t2b, t2.c t2c from t1 left join t2 on (t1.a = t2.a)) t on (t1a = t3.a) WHERE (t2a IS NULL OR (t1c = t3.a));
select * from t3 join (select t1.a t1a, t1.b t1b, t1.c t1c, t2.a t2a, t2.b t2b, t2.c t2c from t1 left join t2 on (t1.a = t2.a)) t on (t1a = t3.a) WHERE (t2a IS NULL OR (t1c = t3.a));

-- ensure plan has a filter over left outer join
explain select * from (select t1.a t1a, t1.b t1b, t2.a t2a, t2.b t2b from t1 left join t2 on t1.a = t2.a) tt 
  join t3 on tt.t1b = t3.b 
  join (select t1.a t1a, t1.b t1b, t2.a t2a, t2.b t2b from t1 left join t2 on t1.a = t2.a) tt1 on tt1.t1b = t3.b 
  join t3 t3_1 on tt1.t1b = t3_1.b and (tt1.t2a is NULL OR tt1.t1b = t3.b);

select * from (select t1.a t1a, t1.b t1b, t2.a t2a, t2.b t2b from t1 left join t2 on t1.a = t2.a) tt 
  join t3 on tt.t1b = t3.b 
  join (select t1.a t1a, t1.b t1b, t2.a t2a, t2.b t2b from t1 left join t2 on t1.a = t2.a) tt1 on tt1.t1b = t3.b 
  join t3 t3_1 on tt1.t1b = t3_1.b and (tt1.t2a is NULL OR tt1.t1b = t3.b);

drop table t1, t2, t3;
-- test that flow->hashExpr variables can be resolved
CREATE TABLE hexpr_t1 (c1 int, c2 character varying(16)) DISTRIBUTED BY (c1);
CREATE TABLE hexpr_t2 (c3 character varying(16)) DISTRIBUTED BY (c3);
INSERT INTO hexpr_t1 SELECT i, i::character varying FROM generate_series(1,10)i;
INSERT INTO hexpr_t2 SELECT i::character varying FROM generate_series(1,10)i;
EXPLAIN SELECT btrim(hexpr_t1.c2::text)::character varying AS foo FROM hexpr_t1 LEFT JOIN hexpr_t2
ON hexpr_t2.c3::text = btrim(hexpr_t1.c2::text);
EXPLAIN SELECT btrim(hexpr_t1.c2::text)::character varying AS foo FROM hexpr_t1 LEFT JOIN hexpr_t2
ON hexpr_t2.c3::text = btrim(hexpr_t1.c2::text);
EXPLAIN SELECT btrim(hexpr_t1.c2::text)::character varying AS foo FROM hexpr_t1 LEFT JOIN hexpr_t2
ON hexpr_t2.c3::text = btrim(hexpr_t1.c2::text);
SELECT btrim(hexpr_t1.c2::text)::character varying AS foo FROM hexpr_t1 LEFT JOIN hexpr_t2
ON hexpr_t2.c3::text = btrim(hexpr_t1.c2::text);
SELECT btrim(hexpr_t1.c2::text)::character varying AS foo FROM hexpr_t1 LEFT JOIN hexpr_t2
ON hexpr_t2.c3::text = btrim(hexpr_t1.c2::text);
SELECT btrim(hexpr_t1.c2::text)::character varying AS foo FROM hexpr_t1 LEFT JOIN hexpr_t2
ON hexpr_t2.c3::text = btrim(hexpr_t1.c2::text);

-- test if subquery locus is general, then
-- we should keep it general
set enable_hashjoin to on;
set enable_mergejoin to off;
set enable_nestloop to off;
create table t_randomly_dist_table(c int) distributed randomly;
-- the following plan should not contain redistributed motion (for planner)
explain
select * from (
  select a from length('123')a
  union all
  select a from length('123')a
) t_subquery_general
join t_randomly_dist_table on t_subquery_general.a = t_randomly_dist_table.c;

reset enable_hashjoin;
reset enable_mergejoin;
reset enable_nestloop;

-- test the scenario that the nestloop/merge join is in a subquery and the
-- parameter passed to the outer of the join. We should not squelch the inner
-- if the outer doesn't return a qualified tuple, because squelch the inner
-- would stop the motion and when the next parameter comes in, the inner rescan
-- can't get the correct data.

set optimizer=off;
create table t_join_squelch_a (a int, b int);
insert into t_join_squelch_a values (2, 2);
create table t_join_squelch_b (x int, y int);
insert into t_join_squelch_b values (2, 2);
create table t_join_squelch_c(i int);
insert into t_join_squelch_c values (1), (2);
analyze t_join_squelch_a;
analyze t_join_squelch_b;
analyze t_join_squelch_c;

set enable_nestloop to on;
set enable_hashjoin to off;
set enable_mergejoin to off;

explain select (select count(*) from (select 'random' from t_join_squelch_a a join t_join_squelch_b b on a.a = b.x and a.b = c.i) x)d from t_join_squelch_c c;
select (select count(*) from (select 'random' from t_join_squelch_a a join t_join_squelch_b b on a.a = b.x and a.b = c.i) x)d from t_join_squelch_c c;

set enable_nestloop to off;
set enable_hashjoin to off;
set enable_mergejoin to on;

explain select (select count(*) from (select 'random' from t_join_squelch_a a join t_join_squelch_b b on a.a = b.x and a.b = c.i) x)d from t_join_squelch_c c;
select (select count(*) from (select 'random' from t_join_squelch_a a join t_join_squelch_b b on a.a = b.x and a.b = c.i) x)d from t_join_squelch_c c;
drop table t_join_squelch_a, t_join_squelch_b, t_join_squelch_c;
reset optimizer;
reset enable_nestloop;
reset enable_hashjoin;
reset enable_mergejoin;

-- test prefetch join qual
-- we do not handle this correct
-- the only case we need to prefetch join qual is:
--   1. outer plan contains motion
--   2. the join qual contains subplan that contains motion
reset client_min_messages;
set Test_print_prefetch_joinqual = true;
-- prefetch join qual is only set correct for planner
set optimizer = off;

create table t1_test_pretch_join_qual(a int, b int, c int);
create table t2_test_pretch_join_qual(a int, b int, c int);

-- the following plan contains redistribute motion in both inner and outer plan
-- the join qual is t1.c > t2.c, it contains no motion, should not prefetch
explain select * from t1_test_pretch_join_qual t1 join t2_test_pretch_join_qual t2
on t1.b = t2.b and t1.c > t2.c;

create table t3_test_pretch_join_qual(a int, b int, c int);

-- the following plan contains motion in both outer plan and join qual,
-- so we should prefetch join qual
explain select * from t1_test_pretch_join_qual t1 join t2_test_pretch_join_qual t2
on t1.b = t2.b and t1.a > any (select sum(b) from t3_test_pretch_join_qual t3 where c > t2.a);

reset Test_print_prefetch_joinqual;
reset optimizer;
