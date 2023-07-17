-- Extra GPDB tests for joins.

-- Ignore "workfile compresssion is not supported by this build" (see
-- 'zlib' test):
--
-- start_matchignore
-- m/ERROR:  workfile compresssion is not supported by this build/
-- end_matchignore

--
-- test numeric hash join
--

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

-- Test rescannable hashjoin with spilling hashtable
set statement_mem='1000kB';
set gp_workfile_compression = off;
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

-- Test rescannable hashjoin with spilling hashtable, with compression
set gp_workfile_compression = on;
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

-- Test to ensure that for full outer join on varchar columns, planner is successful in finding a sort operator in the catalog
create table input_table(a varchar(30), b varchar(30)) distributed by (a);
set enable_hashjoin = off;
explain (costs off) select X.a from input_table X full join (select a from input_table) Y ON X.a = Y.a;

-- Cleanup
reset enable_hashjoin;
set client_min_messages='warning'; -- silence drop-cascade NOTICEs
drop schema pred cascade;
reset search_path;

-- github issue 5370 cases
drop table if exists t5370;
drop table if exists t5370_2;
create table t5370(id int,name text) distributed by(id);
insert into t5370 select i,i from  generate_series(1,1000) i;
create table t5370_2 as select * from t5370 distributed by (id);
analyze t5370_2;
analyze t5370;
explain select * from t5370 a , t5370_2 b where a.name=b.name;

drop table t5370;
drop table t5370_2;

-- github issue 6215 cases
-- When executing the following plan
-- ```
--  Gather Motion 1:1  (slice1; segments: 1)
--    ->  Merge Full Join
--         ->  Seq Scan on int4_tbl a
--         ->  Seq Scan on int4_tbl b
--```
-- Greenplum will raise an Assert Fail.
-- We force adding a material node for
-- merge full join on true.
drop table if exists t6215;
create table t6215(f1 int4) distributed replicated;
insert into t6215(f1) values (1), (2), (3);

set enable_material = off;
-- The plan still have Material operator
explain (costs off) select * from t6215 a full join t6215 b on true;
select * from t6215 a full join t6215 b on true;

drop table t6215;


--
-- This tripped an assertion while deciding the locus for the joins.
-- The code was failing to handle join between SingleQE and Hash correctly,
-- when there were join order restricitions. (see
-- https://github.com/greenplum-db/gpdb/issues/6643
--
select a.f1, b.f1, t.thousand, t.tenthous from
  (select sum(f1) as f1 from int4_tbl i4b) b
   left outer join
     (select sum(f1)+1 as f1 from int4_tbl i4a) a ON a.f1 = b.f1
      left outer join
        tenk1 t ON b.f1 = t.thousand and (a.f1+b.f1+999) = t.tenthous;


-- tests to ensure that join reordering of LOJs and inner joins produces the
-- correct join predicates & residual filters
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
explain (costs off) select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a IS NULL OR (t1.c = t3.c));
select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a IS NULL OR (t1.c = t3.c));

-- ensure plan has two inner joins with the where clause & join predicates ANDed
explain (costs off) select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a = t3.a);
select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a = t3.a);

-- ensure plan has a filter over left outer join
explain (costs off) select * from t1 left join t2 on (t1.a = t2.a) join t3 on (t1.b = t3.b) where (t2.a is distinct from t3.a);
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

-- test different join order enumeration methods
set optimizer_join_order = query;
select * from t1 join t2 on t1.a = t2.a join t3 on t1.b = t3.b;
set optimizer_join_order = greedy;
select * from t1 join t2 on t1.a = t2.a join t3 on t1.b = t3.b;
set optimizer_join_order = exhaustive;
select * from t1 join t2 on t1.a = t2.a join t3 on t1.b = t3.b;
set optimizer_join_order = exhaustive2;
select * from t1 join t2 on t1.a = t2.a join t3 on t1.b = t3.b;
reset optimizer_join_order;
select * from t1 join t2 on t1.a = t2.a join t3 on t1.b = t3.b;

drop table t1, t2, t3;

--
-- Test a bug that nestloop path previously can not generate motion above
-- index path, which sometimes is wrong (this test case is an example).
-- We now depend on parameterized path related variables to judge instead.
-- We conservatively disallow motion when there is parameter requirement
-- for either outer or inner at this moment though there could be room
-- for further improvement (e.g. referring subplan code to do broadcast
-- for base rel if needed, which needs much effort and does not seem to
-- be deserved given we will probably refactor related code for the lateral
-- support in the near future). For the query and guc settings below, Postgres
-- planner can not generate a plan.
set enable_nestloop = 1;
set enable_material = 0;
set enable_seqscan = 0;
set enable_bitmapscan = 0;
explain select tenk1.unique2 >= 0 from tenk1 left join tenk2 on true limit 1;
select tenk1.unique2 >= 0 from tenk1 left join tenk2 on true limit 1;
reset enable_nestloop;
reset enable_material;
reset enable_seqscan;
reset enable_bitmapscan;

-- Below test cases are for planner's cdbpath_motion_for_join, so we close
-- ORCA temporarily.
set optimizer = off;
-- test outer join for general locus
-- replicated table's locus is SegmentGeneral
create table trep_join_gp (c1 int, c2 int) distributed replicated;
-- hash distributed table's locus is Hash
create table thash_join_gp (c1 int, c2 int) distributed by (c1);
-- randomly distributed table's locus is Strewn
create table trand_join_gp (c1 int, c2 int) distributed randomly;
-- start_ignore
create extension if not exists gp_debug_numsegments;
select gp_debug_set_create_table_default_numsegments(1);
-- end_ignore
-- the following replicated table's numsegments is 1
create table trep1_join_gp (c1 int, c2 int) distributed replicated;

insert into trep_join_gp values (1, 1), (2, 2);
insert into thash_join_gp values (1, 1), (2, 2);
insert into trep1_join_gp values (1, 1), (2, 2);

analyze trep_join_gp;
analyze thash_join_gp;
analyze trep1_join_gp;
analyze trand_join_gp;

-- This test is to check that: general left join segmentGeneral --> segmentGeneral
-- And segmentGeneral join hash does not need motion.
explain select * from generate_series(1, 5) g left join trep_join_gp on g = trep_join_gp.c1 join thash_join_gp on true;
select * from generate_series(1, 5) g left join trep_join_gp on g = trep_join_gp.c1 join thash_join_gp on true;

-- The following 4 tests are to check that general left join partition, we could redistribute the
-- general-locus relation when the filter condition is suitable. If we can redistributed
-- general-locus relation, we should not gather them to singleQE.
explain select * from generate_series(1, 5) g left join thash_join_gp on g = thash_join_gp.c1;
select * from generate_series(1, 5) g left join thash_join_gp on g = thash_join_gp.c1;

explain select * from generate_series(1, 5) g left join thash_join_gp on g = thash_join_gp.c2;
select * from generate_series(1, 5) g left join thash_join_gp on g = thash_join_gp.c2;

explain select * from generate_series(1, 5) g left join trand_join_gp on g = trand_join_gp.c1;
select * from generate_series(1, 5) g left join trand_join_gp on g = trand_join_gp.c1;

explain select * from generate_series(1, 5) g full join trand_join_gp on g = trand_join_gp.c1;
select * from generate_series(1, 5) g full join trand_join_gp on g = trand_join_gp.c1;

-- The following 3 tests are to check that segmentGeneral left join partition
-- we could redistribute the segment general-locus relation when the filter condition
-- is suitable. If we can redistributed general-locus relation, we should not
-- gather them to singleQE.
explain select * from trep_join_gp left join thash_join_gp using (c1);
select * from trep_join_gp left join thash_join_gp using (c1);

explain select * from trep_join_gp left join trand_join_gp using (c1);
select * from trep_join_gp left join trand_join_gp using (c1);

explain select * from trep1_join_gp join thash_join_gp using (c1);
select * from trep1_join_gp join thash_join_gp using (c1);

drop table trep_join_gp;
drop table thash_join_gp;
drop table trand_join_gp;
drop table trep1_join_gp;

select gp_debug_set_create_table_default_numsegments(3);

reset optimizer;

-- The following cases are to test planner join size estimation
-- so we need optimizer to be off.
-- When a partition table join other table using partition key,
-- planner should use root table's stat info instead of largest
-- partition's.
reset enable_hashjoin;
reset enable_mergejoin;
reset enable_nestloop;

create table t_joinsize_1 (c1 int, c2 int)
distributed by (c1)
partition by range (c2)
( start (0) end (5) every (1),
  default partition extra );

create table t_joinsize_2 (c1 int, c2 int)
distributed by (c1);

create table t_joinsize_3 (c int) distributed randomly;

insert into t_joinsize_1 select i, i%5 from generate_series(1, 200)i;
insert into t_joinsize_1 select 1, null from generate_series(1, 1000);

insert into t_joinsize_2 select i,i%5 from generate_series(1, 1000)i;
insert into t_joinsize_3 select * from generate_series(1, 100);

analyze t_joinsize_1;
analyze t_joinsize_2;
analyze t_joinsize_3;

-- the following query should not broadcast the join result of t_joinsize_1, t_joinsize_2.
explain select * from (t_joinsize_1 join t_joinsize_2 on t_joinsize_1.c2 = t_joinsize_2.c2) join t_joinsize_3 on t_joinsize_3.c = t_joinsize_1.c1;

drop table t_joinsize_1;
drop table t_joinsize_2;
drop table t_joinsize_3;

-- test if subquery locus is general, then
-- we should keep it general
create table t_randomly_dist_table(c int) distributed randomly;

-- force_explain
-- the following plan should not contain redistributed motion (for planner)
explain
select * from (
  select a from generate_series(1, 10)a
  union all
  select a from generate_series(1, 10)a
) t_subquery_general
join t_randomly_dist_table on t_subquery_general.a = t_randomly_dist_table.c;

drop table t_randomly_dist_table;


-- test lateral join inner plan contains limit
-- we cannot pass params across motion so we
-- can only generate a plan to gather all the
-- data to singleQE. Here we create a compound
-- data type as params to pass into inner plan.
-- By doing so, if we fail to pass correct params
-- into innerplan, it will throw error because
-- of nullpointer reference. If we only use int
-- type as params, the nullpointer reference error
-- may not happen because we parse null to integer 0.

create type mytype_for_lateral_test as (x int, y int);
create table t1_lateral_limit(a int, b int, c mytype_for_lateral_test);
create table t2_lateral_limit(a int, b int);
insert into t1_lateral_limit values (1, 1, '(1,1)');
insert into t1_lateral_limit values (1, 2, '(2,2)');
insert into t2_lateral_limit values (2, 2);
insert into t2_lateral_limit values (3, 3);

explain select * from t1_lateral_limit as t1 cross join lateral
(select ((c).x+t2.b) as n  from t2_lateral_limit as t2 order by n limit 1)s;

select * from t1_lateral_limit as t1 cross join lateral
(select ((c).x+t2.b) as n  from t2_lateral_limit as t2 order by n limit 1)s;

-- Continue with the above cases, if the lateral subquery contains union all
-- and in some of its appendquerys contain limit, it may also lead to bad plan.
-- The best solution may be to walk the query to and do some static analysis
-- to find out which rel has to be gathered and materialized. But it is complicated
-- to do so and this seems less efficient. I believe in future we should do big
-- refactor to make greenplum support lateral well so now, let's just make sure
-- we will not panic.
explain (costs off) select * from t1_lateral_limit as t1 cross join lateral
((select ((c).x+t2.b) as n  from t2_lateral_limit as t2 order by n limit 1) union all select 1)s;

select * from t1_lateral_limit as t1 cross join lateral
((select ((c).x+t2.b) as n  from t2_lateral_limit as t2 order by n limit 1) union all select 1)s;

-- test lateral subquery contains group by (group-by is another place that
-- may add motions in the subquery's plan).
explain select * from t1_lateral_limit t1 cross join lateral
(select (c).x+t2.a, sum(t2.a+t2.b) from t2_lateral_limit t2 group by (c).x+t2.a)x;

select * from t1_lateral_limit t1 cross join lateral
(select (c).x+t2.a, sum(t2.a+t2.b) from t2_lateral_limit t2 group by (c).x+t2.a)x;

-- The following case is from Github Issue
-- https://github.com/greenplum-db/gpdb/issues/8860
-- It is the same issue as the above test suite.
create table t_mylog_issue_8860 (myid int, log_date timestamptz );
insert into  t_mylog_issue_8860 values (1,timestamptz '2000-01-02 03:04'),(1,timestamptz '2000-01-02 03:04'-'1 hour'::interval);
insert into  t_mylog_issue_8860 values (2,timestamptz '2000-01-02 03:04'),(2,timestamptz '2000-01-02 03:04'-'2 hour'::interval);

explain select ml1.myid, log_date as first_date, ml2.next_date from t_mylog_issue_8860 ml1
inner join lateral
(select myid, log_date as next_date
 from t_mylog_issue_8860 where myid = ml1.myid and log_date > ml1.log_date order by log_date asc limit 1) ml2
on true;

select ml1.myid, log_date as first_date, ml2.next_date from t_mylog_issue_8860 ml1
inner join lateral
(select myid, log_date as next_date
 from t_mylog_issue_8860 where myid = ml1.myid and log_date > ml1.log_date order by log_date asc limit 1) ml2
on true;

-- Github Issue: https://github.com/greenplum-db/gpdb/issues/9733
-- Previously in the function bring_to_outer_query and
-- bring_to_singleQE it depends on the path->param_info field
-- to determine if the path contains outerParams. This is not
-- enought. The following case would SegFault before because
-- the indexpath's orderby clause contains outerParams.
create table gist_tbl_github9733 (b box, p point, c circle);
insert into gist_tbl_github9733
select box(point(0.05*i, 0.05*i), point(0.05*i, 0.05*i)),
       point(0.05*i, 0.05*i),
       circle(point(0.05*i, 0.05*i), 1.0)
from generate_series(0,10000) as i;
vacuum analyze gist_tbl_github9733;
create index gist_tbl_point_index_github9733 on gist_tbl_github9733 using gist (p);
set enable_seqscan=off;
set enable_bitmapscan=off;
explain (costs off)
select p from
  (values (box(point(0,0), point(0.5,0.5))),
          (box(point(0.5,0.5), point(0.75,0.75))),
          (box(point(0.8,0.8), point(1.0,1.0)))) as v(bb)
cross join lateral
  (select p from gist_tbl_github9733 where p <@ bb order by p <-> bb[0] limit 2) ss;

reset enable_seqscan;
explain (costs off)
select p from
  (values (box(point(0,0), point(0.5,0.5))),
          (box(point(0.5,0.5), point(0.75,0.75))),
          (box(point(0.8,0.8), point(1.0,1.0)))) as v(bb)
cross join lateral
  (select p from gist_tbl_github9733 where p <@ bb order by p <-> bb[0] limit 2) ss;

select p from
  (values (box(point(0,0), point(0.5,0.5))),
          (box(point(0.5,0.5), point(0.75,0.75))),
          (box(point(0.8,0.8), point(1.0,1.0)))) as v(bb)
cross join lateral
  (select p from gist_tbl_github9733 where p <@ bb order by p <-> bb[0] limit 2) ss;

reset enable_bitmapscan;

---
--- Test that GUC enable_hashagg takes effect for SEMI join
---
drop table if exists foo;
drop table if exists bar;

create table foo(a int) distributed by (a);
create table bar(b int) distributed by (b);

insert into foo select i from generate_series(1,10)i;
insert into bar select i from generate_series(1,1000)i;

analyze foo;
analyze bar;

set enable_hashagg to on;
explain (costs off)
select * from foo where exists (select 1 from bar where foo.a = bar.b);
select * from foo where exists (select 1 from bar where foo.a = bar.b);

set enable_hashagg to off;
explain (costs off)
select * from foo where exists (select 1 from bar where foo.a = bar.b);
select * from foo where exists (select 1 from bar where foo.a = bar.b);

reset enable_hashagg;
drop table foo;
drop table bar;

-- Fix github issue 10012
create table fix_param_a (i int, j int);
create table fix_param_b (i int UNIQUE, j int);
create table fix_param_c (i int, j int);

insert into fix_param_a select i, i from generate_series(1,20)i;
insert into fix_param_b select i, i from generate_series(1,2000)i;
insert into fix_param_c select i, i from generate_series(1,2000)i;

analyze fix_param_a;
analyze fix_param_b;
analyze fix_param_c;

explain (costs off)
select * from fix_param_a left join fix_param_b on
	fix_param_a.i = fix_param_b.i and fix_param_b.j in
		(select j from fix_param_c where fix_param_b.i = fix_param_c.i)
	order by 1;
select * from fix_param_a left join fix_param_b on
	fix_param_a.i = fix_param_b.i and fix_param_b.j in
		(select j from fix_param_c where fix_param_b.i = fix_param_c.i)
	order by 1;

-- Test targetlist contains placeholder var
-- When creating a redistributed motion with hash keys,
-- Greenplum planner will invoke `cdbpullup_findEclassInTargetList`.
-- The following test case contains non-strict function `coalesce`
-- in the subquery at nullable-side of outerjoin and thus will
-- have PlaceHolderVar in targetlist. The case is to test if
-- function `cdbpullup_findEclassInTargetList` handles PlaceHolderVar
-- correct.
-- See github issue: https://github.com/greenplum-db/gpdb/issues/10315
create table t_issue_10315 ( id1 int, id2 int );

insert into t_issue_10315 select i,i from generate_series(1, 2)i;
insert into t_issue_10315 select i,null from generate_series(1, 2)i;
insert into t_issue_10315 select null,i from generate_series(1, 2)i;

select *  from
( select coalesce( bq.id1 ) id1, coalesce ( bq.id2 ) id2
        from ( select r.id1, r.id2 from t_issue_10315 r group by r.id1, r.id2 ) bq  ) t
full join ( select r.id1, r.id2 from t_issue_10315 r group by r.id1, r.id2 ) bq_all
on t.id1 = bq_all.id1  and t.id2 = bq_all.id2
full join ( select r.id1, r.id2 from t_issue_10315 r group by r.id1, r.id2 ) tq_all
on (coalesce(t.id1) = tq_all.id1  and t.id2 = tq_all.id2) ;

drop table t_issue_10315;
--
-- Left Join Pruning --
-- Cases when join will be pruned--
-- Single Unique key in inner relation --
create table fooJoinPruning (a int,b int,c int,constraint idx1 unique(a));
create table barJoinPruning (p int,q int,r int,constraint idx2 unique(p));
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation or is a constant --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on barJoinPruning.p=100  where fooJoinPruning.b>300;
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.c=barJoinPruning.p  where fooJoinPruning.b>300;
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation and filter contains subquery--
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.c=barJoinPruning.p  where fooJoinPruning.b>300 and fooJoinPruning.c in (select barJoinPruning.q from barJoinPruning );
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.c=barJoinPruning.p where fooJoinPruning.b>300 and fooJoinPruning.c > ANY (select barJoinPruning.q from barJoinPruning );
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation and filter contains corelated subquery referencing outer relation column--
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.c=barJoinPruning.p  where fooJoinPruning.b in (select fooJoinPruning.a from barJoinPruning);
drop table fooJoinPruning;
drop table barJoinPruning;
-- MultipleUnique key sets  in inner relation --
create table fooJoinPruning (a int, b int, c int,d int,e int,f int,g int,constraint idx1 unique(a,b),constraint idx2 unique(a,c,d));
create table barJoinPruning (p int, q int, r int,s int,t int,u int,v int,constraint idx3 unique(p,q),constraint idx4 unique(p,r,s));
create table t1JoinPruning(m int primary key,n int);
create table t2JoinPruning(x int primary key,y int);
-- Unique key set of inner relation ie 'p,q' is present in the join condition and is equal to a column from outer relation or is a constant --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on barJoinPruning.p=100 and barJoinPruning.q=200 where fooJoinPruning.e >300 and fooJoinPruning.f<>10;
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.g=barJoinPruning.p and fooJoinPruning.a=barJoinPruning.q where fooJoinPruning.e >300 and fooJoinPruning.f<>10;
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.g=barJoinPruning.p and barJoinPruning.q=100 where fooJoinPruning.e >300 and fooJoinPruning.f<>10;
-- Unique key set of inner relation ie 'p,r,s' is present in the join condition and is equal to a column from outer relation or is a constant --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.g=barJoinPruning.p and barJoinPruning.r=100 and fooJoinPruning.b=barJoinPruning.s where fooJoinPruning.f<>10;
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.a=barJoinPruning.p and fooJoinPruning.b=barJoinPruning.r and fooJoinPruning.c=barJoinPruning.s and barJoinPruning.s=barJoinPruning.t where fooJoinPruning.b>300;
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation and filter contains subquery --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.g=barJoinPruning.p and fooJoinPruning.a=barJoinPruning.q where fooJoinPruning.c in (select barJoinPruning.t from barJoinPruning );
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.g=barJoinPruning.p and fooJoinPruning.a=barJoinPruning.q where fooJoinPruning.c > ANY (select barJoinPruning.t from barJoinPruning );
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation and filter contains corelated subquery referencing outer relation column --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.g=barJoinPruning.p and fooJoinPruning.a=barJoinPruning.q where fooJoinPruning.e in (select fooJoinPruning.f from barJoinPruning);
-- Prunable Left join present in subquery --
explain select t1JoinPruning.n from t1JoinPruning where t1JoinPruning.m in (select fooJoinPruning.a from fooJoinPruning left join barJoinPruning on barJoinPruning.p=100 and barJoinPruning.q=200);
drop table fooJoinPruning;
drop table barJoinPruning;
drop table t1JoinPruning;
drop table t2JoinPruning;
create table t1 (a int);
create table t2 (a int primary key, b int);
create table t3 (a int primary key, b int);
-- inner table is join
EXPLAIN select t1.a from t1 left join (t2 join t3 on true) on t2.a=t1.a and t3.a=t1.a;
-- inner table has new left join
EXPLAIN select t1.* from t1 left join (t2 left join t3 on t3.a=t2.b) on t2.a=t1.a;
-- inner table is a derived table
EXPLAIN
select t1.* from t1 left join
                 (
                     select t2.b as v2b, count(*) as v2c
                     from t2 left join t3 on t3.a=t2.b
                     group by t2.b
                 ) v2
                 on v2.v2b=t1.a;
drop table t1;
drop table t2;
drop table t3;
--
-- Cases where join will not be pruned
--
-- Single Unique key in inner relation --
create table fooJoinPruning (a int,b int,c int,constraint idx1 unique(a));
create table barJoinPruning (p int,q int,r int,constraint idx2 unique(p));
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation but filter is on a inner relation --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on barJoinPruning.p=fooJoinPruning.b  where barJoinPruning.q<>10;
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation but output columns are from inner relation --
explain select barJoinPruning.* from fooJoinPruning left join barJoinPruning on barJoinPruning.p=fooJoinPruning.b  where fooJoinPruning.b>1000;
-- Subquery present in join condition
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on barJoinPruning.p in (select fooJoinPruning.b from fooJoinPruning  ) where fooJoinPruning.c>100;
-- Unique key of inner relation ie 'p' is present in the join condition and is equal to a column from outer relation and filter contains corelated subquery referencing inner relation column--
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.c=barJoinPruning.p  where fooJoinPruning.b in (select barJoinPruning.q from fooJoinPruning);
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.c=barJoinPruning.p  where fooJoinPruning.b in (select barJoinPruning.q from fooJoinPruning where fooJoinPruning.a=barJoinPruning.r);
drop table fooJoinPruning;
drop table barJoinPruning;
-- Multiple Unique key sets  in inner relation --
create table fooJoinPruning (a int, b int, c int,d int,e int,f int,g int,constraint idx1 unique(a,b),constraint idx2 unique(a,c,d));
create table barJoinPruning (p int, q int, r int,s int,t int,u int,v int,constraint idx3 unique(p,q),constraint idx4 unique(p,r,s));
-- No equality operator present in join condition --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on barJoinPruning.p>100 and barJoinPruning.q>200  where fooJoinPruning.b>300;
-- OR operator is present in join condition --
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.a=barJoinPruning.p and fooJoinPruning.c=barJoinPruning.r or fooJoinPruning.d=barJoinPruning.s;
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.a=barJoinPruning.p or fooJoinPruning.b=barJoinPruning.q  where fooJoinPruning.b>300;
-- Not all unique keys of inner relation are equal to a constant or column from outer relation
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.a=barJoinPruning.p and barJoinPruning.r=barJoinPruning.s;
explain select fooJoinPruning.* from fooJoinPruning left join barJoinPruning on fooJoinPruning.a=barJoinPruning.p and fooJoinPruning.b=barJoinPruning.r and barJoinPruning.s=barJoinPruning.t where fooJoinPruning.b>300;
drop table fooJoinPruning;
drop table barJoinPruning;

-----------------------------------------------------------------
-- Test cases on Dynamic Partition Elimination(DPE) for Right Joins
-----------------------------------------------------------------

-- Note1 : DPE for Right join will happen if, all the following satisfy
-- Condition 1: Outer table is partition table
-- Condition 2: The partitioned column is same as distribution column
-- Condition 3: Join condition is on partitioned key of outer table

-- Note2 : To view the effect of DPE, the queries should be run with
-- "Explain Analyze ...". With it, the exact number of partitions scanned
-- will be shows in the plan.
-- Eg: explain analyze select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a;

drop table if exists foo;
drop table if exists bar_PT1;
drop table if exists bar_PT2;
drop table if exists bar_PT3;
drop table if exists bar_List_PT1;
drop table if exists bar_List_PT2;

-- Table creation : Normal table
create table foo (a int , b int) distributed by (a);
insert into foo select i,i from generate_series(1,5)i;
analyze foo;
-- Table creation : First range Partitioned table with same 'Distribution Column' and 'Partitioning key'
create table bar_PT1 (a1_PC int, b1 int) partition by range(a1_PC) (start (1) inclusive end (12) every (2)) distributed by (a1_PC);
insert into bar_PT1 select i,i from generate_series(1,11)i;
analyze bar_PT1;
-- Table creation : Second range Partitioned table with different 'Distribution Column' and 'Partitioning key'
create table bar_PT2 (a2 int, b2_PC int) partition by range(b2_PC) (start (1) inclusive end (12) every (2)) distributed by (a2);
insert into bar_PT2 select i,i from generate_series(1,11)i;
analyze bar_PT2;
-- Table creation : Third range Partitioned table with same 'Distribution Column' and 'Partitioning key'
create table bar_PT3 (a3_PC int, b3 int) partition by range(a3_PC) (start (1) inclusive end (6) every (2))distributed by (a3_PC);
insert into bar_PT3 select i,i from generate_series(1,5)i;
analyze bar_PT3;

-- Table creation : First list Partitioned table with same 'Distribution Column' and 'Partitioning key'
create table bar_List_PT1 (a1_PC int, b1 int) partition by list(a1_PC)
(partition p1 values(1,2), partition p2 values(3,4), partition p3 values(5,6), partition p4 values(7,8), partition p5 values(9,10),
 partition p6 values(11,12), partition p7 values(13,14), partition p8 values(15,16), partition p9 values(17,18), partition p10 values(19,20),
 partition p11 values(21,22), partition p12 values(23,24), default partition pdefault) distributed by (a1_PC);
insert into bar_List_PT1 select i,i from generate_series(1,24)i;
analyze bar_List_PT1;

-- Table creation : Second list Partitioned table with same 'Distribution Column' and 'Partitioning key'
create table bar_List_PT2 (a2_PC int, b2 int) partition by list(a2_PC)
 (partition p1 values(1,2), partition p2 values(3,4), partition p3 values(5,6), partition p4 values(7,8), partition p5 values(9,10),
 partition p6 values(11,12), default partition pdefault) distributed by (a2_PC);
insert into bar_List_PT2 select i,i from generate_series(1,12)i;
analyze bar_List_PT2;

-- Case-1 : Distribution colm = Partition Key.

-- FOR RANGE PARTITIONED TABLE

-- Outer table: Partitioned table, Join Condition on Partition key: Yes, Result: DPE - YES
explain (costs off) select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a;
select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a;
-- Outer table: Partitioned table, Join Condition on Partition key: No, Result: DPE - No
explain (costs off) select * from bar_PT1 right join foo on bar_PT1.b1 =foo.a;
select * from bar_PT1 right join foo on bar_PT1.b1 =foo.a;
-- Outer,Inner table: Partitioned table, Join Condition on Partition key: Yes, Result: DPE - Yes
explain (costs off) select * from bar_PT1 right join bar_PT3 on bar_PT1.a1_PC =bar_PT3.a3_PC;
select * from bar_PT1 right join bar_PT3 on bar_PT1.a1_PC =bar_PT3.a3_PC;
-- Outer table: Not a Partitioned table, Join Condition on Partition key: Yes, Result: DPE - No
explain (costs off) select * from foo right join bar_PT1 on foo.a=bar_PT1.a1_PC;
select * from foo right join bar_PT1 on foo.a=bar_PT1.a1_PC;
-- Right join with predicate on the column of non partitioned table in 'where clause'.
-- Result: DPE - Yes,
explain (costs off) select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a where foo.a>2;
select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a where foo.a>2;
--Conjunction in join condition, Result: DPE - Yes
explain (costs off) select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a and bar_PT1.b1 =foo.b;
select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a and bar_PT1.b1 =foo.b;

explain (costs off) select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a and foo.b>2;
select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a and foo.b>2;

-- Multiple Right Joins, DPE- Yes
explain (costs off) select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a right join bar_PT2 on bar_PT1.a1_PC =bar_PT2.b2_PC;
select * from bar_PT1 right join foo on bar_PT1.a1_PC =foo.a right join bar_PT2 on bar_PT1.a1_PC =bar_PT2.b2_PC;

-- FOR LIST PARTITIONED TABLE

-- Outer table: List Partitioned table, Join Condition on Partition key: Yes, Result: DPE - YES
explain (costs off) select * from bar_List_PT1 right join foo on bar_List_PT1.a1_PC =foo.a;
select * from bar_List_PT1 right join foo on bar_List_PT1.a1_PC =foo.a;

-- Outer,Inner table: Partitioned table, Join Condition on Partition key: Yes, Result: DPE - Yes
explain (costs off) select * from bar_List_PT1 right join bar_List_PT2 on bar_List_PT1.a1_PC =bar_List_PT2.a2_PC;
select * from bar_List_PT1 right join bar_List_PT2 on bar_List_PT1.a1_PC =bar_List_PT2.a2_PC;

-- Case-2 : Distribution colm <> Partition Key.

-- Outer table: Partitioned table, Join Condition on Partition key: Yes, Result: DPE - No
explain (costs off) select * from bar_PT2 right join foo on bar_PT2.b2_PC =foo.a;
select * from bar_PT2 right join foo on bar_PT2.b2_PC =foo.a;
-- Outer,Inner table: Partitioned table, Join Condition on Partition key: Yes, Result: DPE - No
explain (costs off) select * from bar_PT2 right join bar_PT1 on bar_PT2.b2_PC =bar_PT1.b1;
select * from bar_PT2 right join bar_PT1 on bar_PT2.b2_PC =bar_PT1.b1;

drop table if exists foo;
drop table if exists bar_PT1;
drop table if exists bar_PT2;
drop table if exists bar_PT3;
drop table if exists bar_List_PT1;
drop table if exists bar_List_PT2;

create table foo_varchar (a varchar(5)) distributed by (a);
create table bar_char (p char(5)) distributed by (p);
create table random_dis_varchar (x varchar(5)) distributed randomly;
create table random_dis_char (y char(5)) distributed randomly;

insert into foo_varchar values ('1 '),('2  '),('3   ');
insert into bar_char values ('1 '),('2  '),('3   ');
insert into random_dis_varchar values ('1 '),('2  '),('3   ');
insert into random_dis_char values ('1 '),('2  '),('3   ');

set optimizer_enable_hashjoin to off;
set enable_hashjoin to off;
set enable_nestloop to on;

-- check motion is added when performing a NL Left Outer Join between relations
-- when the join condition columns belong to different opfamily and both are
-- distribution keys
explain select * from foo_varchar left join bar_char on foo_varchar.a=bar_char.p;
select * from foo_varchar left join bar_char on foo_varchar.a=bar_char.p;

-- There is a plan change (from redistribution to broadcast) because a NULL
-- matching distribution is returned when there is opfamily mismatch between join
-- columns.
explain select * from foo_varchar left join random_dis_char on foo_varchar.a=random_dis_char.y;
select * from foo_varchar left join random_dis_char on foo_varchar.a=random_dis_char.y;

explain select * from bar_char left join random_dis_varchar on bar_char.p=random_dis_varchar.x;
select * from bar_char left join random_dis_varchar on bar_char.p=random_dis_varchar.x;

-- check motion is added when performing a NL Inner Join between relations when
-- the join condition columns belong to different opfamily and both are
-- distribution keys
explain select * from foo_varchar inner join bar_char on foo_varchar.a=bar_char.p;
select * from foo_varchar inner join bar_char on foo_varchar.a=bar_char.p;

-- There is a plan change (from redistribution to broadcast) because a NULL
-- matching distribution is returned when there is opfamily mismatch between join
-- columns.
explain select * from foo_varchar inner join random_dis_char on foo_varchar.a=random_dis_char.y;
select * from foo_varchar inner join random_dis_char on foo_varchar.a=random_dis_char.y;

explain select * from bar_char inner join random_dis_varchar on bar_char.p=random_dis_varchar.x;
select * from bar_char inner join random_dis_varchar on bar_char.p=random_dis_varchar.x;

drop table foo_varchar;
drop table bar_char;
drop table random_dis_varchar;
drop table random_dis_char;

set optimizer_enable_hashjoin to on;
reset enable_hashjoin;
reset enable_nestloop;
