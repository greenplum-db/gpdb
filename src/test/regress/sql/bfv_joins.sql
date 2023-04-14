--
-- Set up
--
create schema bfv_joins;
set search_path='bfv_joins';

create table x (a int, b int, c int);
insert into x values (generate_series(1,10), generate_series(1,10), generate_series(1,10));
create table y (a int, b int, c int);
insert into y (select * from x);

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (a int, b int);

INSERT INTO t1 VALUES (1,1),(2,1),(3,NULL);
INSERT INTO t2 VALUES (2,3);

CREATE FUNCTION func_x(x int) RETURNS int AS $$
BEGIN
RETURN $1 +1;
END
$$ LANGUAGE plpgsql;

create table z(x int) distributed by (x);

CREATE TABLE bfv_joins_foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
CREATE TABLE bfv_joins_bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
CREATE TABLE t AS SELECT bfv_joins_foo.a,bfv_joins_foo.b,bfv_joins_bar.d FROM bfv_joins_foo,bfv_joins_bar WHERE bfv_joins_foo.a = bfv_joins_bar.d;

CREATE FUNCTION my_equality(a int, b int) RETURNS BOOL
    AS $$ SELECT $1 < $2 $$
    LANGUAGE SQL;

create table x_non_part (a int, b int, c int);
insert into x_non_part select i%3, i, i from generate_series(1,10) i;

create table x_part (e int, f int, g int) partition by range(e) (start(1) end(5) every(1), default partition extra);
insert into x_part select generate_series(1,10), generate_series(1,10) * 3, generate_series(1,10)%6;

analyze x_non_part;
analyze x_part;

--
-- Test with more null-filtering conditions for LOJ transformation in Orca
--
SELECT * from x left join y on True where y.a > 0;

SELECT * from x left join y on True where y.a > 0 and y.b > 0;

SELECT * from x left join y on True where y.a in (1,2,3);

SELECT * from x left join y on True where y.a = y.b ;

SELECT * from x left join y on True where y.a is NULL;

SELECT * from x left join y on True where y.a is NOT NULL;

SELECT * from x left join y on True where y.a is NULL and Y.b > 0;

SELECT * from x left join y on True where func_x(y.a) > 0;

SELECT * FROM t1 LEFT OUTER JOIN t2 ON t1.a = t2.a WHERE t1.b IS DISTINCT FROM t2.b;

SELECT * FROM t1 LEFT OUTER JOIN t2 ON t1.a = t2.a WHERE t1.b IS DISTINCT FROM NULL;

SELECT * FROM t1 LEFT OUTER JOIN t2 ON t1.a = t2.a WHERE t2.b IS DISTINCT FROM NULL;

SELECT * FROM t1 LEFT OUTER JOIN t2 ON t1.a = t2.a WHERE t2.b IS NOT DISTINCT FROM NULL;

SELECT * FROM t1 LEFT OUTER JOIN t2 ON t1.a = t2.a WHERE t1.b IS NOT DISTINCT FROM NULL;

-- Test for unexpected NLJ qual
--
explain select 1 as mrs_t1 where 1 <= ALL (select x from z);

--
-- Test for wrong results in window functions under joins #1
--
select * from
(SELECT bfv_joins_bar.*, AVG(t.b) OVER(PARTITION BY t.a ORDER BY t.b desc) AS e FROM t,bfv_joins_bar) bfv_joins_foo, t
where e < 10
order by 1, 2, 3, 4, 5, 6;

--
-- Test for wrong results in window functions under joins #2
--
select * from (
SELECT cup.*, SUM(t.d) OVER(PARTITION BY t.b) FROM (
	SELECT bfv_joins_bar.*, AVG(t.b) OVER(PARTITION BY t.a ORDER BY t.b desc) AS e FROM t,bfv_joins_bar
) AS cup,
t WHERE cup.e < 10
GROUP BY cup.c,cup.d, cup.e ,t.d, t.b) i
order by 1, 2, 3, 4;

--
-- Test for wrong results in window functions under joins #3
--
select * from (
WITH t(a,b,d) as (SELECT bfv_joins_foo.a,bfv_joins_foo.b,bfv_joins_bar.d FROM bfv_joins_foo,bfv_joins_bar WHERE bfv_joins_foo.a = bfv_joins_bar.d )
SELECT cup.*, SUM(t.d) OVER(PARTITION BY t.b) FROM (
	SELECT bfv_joins_bar.*, AVG(t.b) OVER(PARTITION BY t.a ORDER BY t.b desc) AS e FROM t,bfv_joins_bar
) as cup,
t WHERE cup.e < 10
GROUP BY cup.c,cup.d, cup.e ,t.d,t.b) i
order by 1, 2, 3, 4;

--
-- Query on partitioned table with range join predicate on part key causes fallback to planner
--
select * from x_part, x_non_part where a > e;
select * from x_part, x_non_part where a <> e;
select * from x_part, x_non_part where a <= e;
select * from x_part left join x_non_part on (a > e);
select * from x_part right join x_non_part on (a > e);
select * from x_part join x_non_part on (my_equality(a,e));

--
-- This threw an error at one point:
-- ERROR: FULL JOIN is only supported with merge-joinable join conditions
--
create table fjtest_a (aid oid);
create table fjtest_b (bid oid);
create table fjtest_c (cid oid);

insert into fjtest_a values (0), (1), (2);
insert into fjtest_b values (0), (2), (3);
insert into fjtest_c values (0), (3), (4);

select * from
(
  select * from fjtest_a a, fjtest_b b where (aid = bid)
) s
full outer join fjtest_c on (s.aid = cid);

-- Do not push down any implied predicates to the Left Outer Join
CREATE TABLE member(member_id int NOT NULL, group_id int NOT NULL) DISTRIBUTED BY(member_id);
CREATE TABLE member_group(group_id int NOT NULL) DISTRIBUTED BY(group_id);
CREATE TABLE region(region_id char(4), county_name varchar(25)) DISTRIBUTED BY(region_id);
CREATE TABLE member_subgroup(subgroup_id int NOT NULL, group_id int NOT NULL, subgroup_name text) DISTRIBUTED RANDOMLY;

INSERT INTO region SELECT i, i FROM generate_series(1, 200) i;
INSERT INTO member_group SELECT i FROM generate_series(1, 15) i;
INSERT INTO member SELECT i, i%15 FROM generate_series(1, 10000) i;
--start_ignore
ANALYZE member;
ANALYZE member_group;
ANALYZE region;
ANALYZE member_subgroup;
--end_ignore
EXPLAIN(COSTS OFF) SELECT member.member_id
FROM member
INNER JOIN member_group
ON member.group_id = member_group.group_id
INNER JOIN member_subgroup
ON member_group.group_id = member_subgroup.group_id
LEFT OUTER JOIN region
ON (member_group.group_id IN (12,13,14,15) AND member_subgroup.subgroup_name = region.county_name);

-- Test colocated equijoins on coerced distribution keys
CREATE TABLE coercejoin (a varchar(10), b varchar(10)) DISTRIBUTED BY (a);
-- Positive test, the join should be colocated as the implicit cast from the
-- parse rewrite is a relabeling (varchar::text).
EXPLAIN (costs off) SELECT * FROM coercejoin a, coercejoin b WHERE a.a=b.a;
-- Negative test, the join should not be colocated since the cast is a coercion
-- which cannot guarantee that the coerced value would hash to the same segment
-- as the uncoerced tuple.
EXPLAIN (costs off) SELECT * FROM coercejoin a, coercejoin b WHERE a.a::numeric=b.a::numeric;

--
-- Test NLJ with join conds on distr keys using equality, IS DISTINCT FROM & IS NOT DISTINCT FROM exprs
--
create table nlj1 (a int, b int);
create table nlj2 (a int, b int);

insert into nlj1 values (1, 1), (NULL, NULL);
insert into nlj2 values (1, 5), (NULL, 6);
analyze nlj1;
analyze nlj2;

set optimizer_enable_hashjoin=off;
set enable_hashjoin=off; set enable_mergejoin=off; set enable_nestloop=on;

explain select * from nlj1, nlj2 where nlj1.a = nlj2.a;
select * from nlj1, nlj2 where nlj1.a = nlj2.a;

explain select * from nlj1, nlj2 where nlj1.a is not distinct from nlj2.a;
select * from nlj1, nlj2 where nlj1.a is not distinct from nlj2.a;

explain select * from nlj1, (select NULL::int a, b from nlj2) other where nlj1.a is not distinct from other.a;
select * from nlj1, (select NULL::int a, b from nlj2) other where nlj1.a is not distinct from other.a;

explain select * from nlj1, nlj2 where nlj1.a is distinct from nlj2.a;
select * from nlj1, nlj2 where nlj1.a is distinct from nlj2.a;

reset optimizer_enable_hashjoin;
reset enable_hashjoin; reset enable_mergejoin; reset enable_nestloop;


--
-- At one point, we didn't ensure that the outer side of a NestLoop path
-- was rescannable, if the NestLoop was used on the inner side of another
-- NestLoop.
--
-- See https://github.com/greenplum-db/gpdb/issues/6769.
--
create table a (i int4);
create table b (i int4);
create table c (i int4, j int4);

insert into a select g from generate_series(1,1) g;
insert into b select g from generate_series(1,1) g;
insert into c select g, g from generate_series(1, 100) g;
analyze a;
analyze b;
analyze c;

create index on c (j, i);

-- In order to get the plan we want, Index Scan on 'c' must appear
-- much cheaper than a Seq Scan. In order to keep this test quick and small,
-- we don't want to actually create a huge table, so cheat a little and
-- force that stats to make it look big to the planner.
set allow_system_table_mods = on;

update pg_class set reltuples=1 where oid ='a'::regclass;
update pg_class set relpages=1 where oid ='a'::regclass;

update pg_class set reltuples=10 where oid ='b'::regclass;
update pg_class set relpages=10 where oid ='b'::regclass;

update pg_class set reltuples=10000000 where oid ='c'::regclass;
update pg_class set relpages=100000 where oid ='c'::regclass;

set enable_hashjoin=off;
set enable_mergejoin=off;
set enable_nestloop=on;
set random_page_cost=1;

set join_collapse_limit=1;
set from_collapse_limit=1;

-- the plan should look something like this:
--
--                                 QUERY PLAN                                 
-- ---------------------------------------------------------------------------
--  Gather Motion 3:1  (slice1; segments: 3)
--    ->  Nested Loop [1]
--          ->  Broadcast Motion 3:3  (slice2; segments: 3)
--                ->  Seq Scan on b
--          ->  Materialize  [6]
--                ->  Nested Loop [2]
--                      Join Filter: (b.i = a.i)
--                      ->  Materialize [5]
--                            ->  Broadcast Motion 3:3  (slice3; segments: 3) [3]
--                                  ->  Seq Scan on a
--                      ->  Index Only Scan using c_j_i_idx on c
--                            Index Cond: (j = (a.i + b.i)) [4]
--  Optimizer: Postgres query optimizer
-- (14 rows)
--
-- The crucal parts are:
--
-- * Nested Loop join on the inner side of another Nested Loop join [1], [2]
--
-- * Motion on the outer side of the inner Nested Loop join (the Broadcast
--   Motion on top of "Seq Scan on a" [3])
--
-- * An Index scan in the innermost path, which uses an executor parameter
--   from the outermost path ("b.i", in the Index Cond) [4]
--
-- There must be a Materialize node on top of the "Broadcast Motion -> Seq Scan"
-- path [5]. Otherwise, when the outermost scan on 'b' produces a new row, and
-- the outer Nested Loop calls Rescan on its inner side, the Motion node would
-- be rescanned. Note that the Materialize node at [6] does *not* shield the
-- Motion node from rescanning! That Materialize node is rescanned, when the
-- executor parameter 'b.i' changes.

explain (costs off) select * from b, lateral (select * from a, c where b.i = a.i and (a.i + b.i) = c.j) as ac;

select * from b, lateral (select * from a, c where b.i = a.i and (a.i + b.i) = c.j) as ac;

-- The above plan will prefetch inner plan and the inner plan refers
-- outerParams. Previously, we do not handle this case correct and forgot
-- to set the Params for nestloop in econtext. The outer Param is a compound
-- data type instead of simple integer, it will lead to PANIC.
-- See Github Issue: https://github.com/greenplum-db/gpdb/issues/9679
-- for details.
create type mytype_prefetch_params as (x int, y int);
alter table b add column mt_col mytype_prefetch_params;

explain select ac.*, b.i from b, lateral (select * from a, c where ((mt_col).x > a.i or b.i = a.i) and (a.i + b.i) = c.j) as ac;
select ac.*, b.i from b, lateral (select * from a, c where ((mt_col).x > a.i or b.i = a.i) and (a.i + b.i) = c.j) as ac;

reset enable_hashjoin;
reset enable_mergejoin;
reset enable_nestloop;
reset join_collapse_limit;
reset from_collapse_limit;

--
-- Mix timestamp and timestamptz in a join. We cannot use a Redistribute
-- Motion, because the cross-datatype = operator between them doesn't belong
-- to any hash operator class. We cannot hash rows in a way that matches would
-- land on the same segment in that case.
--
CREATE TABLE gp_timestamp1 (a int, b timestamp, bb timestamptz) DISTRIBUTED BY (a, b);
CREATE TABLE gp_timestamp2 (c int, d timestamp, dd timestamptz) DISTRIBUTED BY (c, d);
INSERT INTO gp_timestamp1 VALUES
  ( 9, '2016/11/09', '2016/11/09'),
  (10, '2016/11/10', '2016/11/10'),
  (11, '2016/11/11', '2016/11/11'),
  (12, '2016/11/12', '2016/11/12'),
  (13, '2016/11/13', '2016/11/13');

INSERT INTO gp_timestamp2 VALUES
  ( 9, '2016/11/09', '2016/11/09'),
  (10, '2016/11/10', '2016/11/10'),
  (11, '2016/11/11', '2016/11/11'),
  (12, '2016/11/12', '2016/11/12'),
  (13, '2016/11/13', '2016/11/13');

ANALYZE gp_timestamp1;
ANALYZE gp_timestamp2;

SELECT a, b FROM gp_timestamp1 JOIN gp_timestamp2 ON a = c AND b = dd AND b = bb AND b = timestamp '2016/11/11';

-- Similar case, but involving a constant
SELECT a, b FROM gp_timestamp1 JOIN gp_timestamp2 ON a = c AND b = timestamptz '2016/11/11';

-- Similar case. Here, the =(float8, float4) cross-type operator would be
-- hashable using the default hash opclass. But not with the legacy cdbhash
-- opclass.
CREATE TABLE gp_float1 (a int, b real) DISTRIBUTED BY (a, b cdbhash_float4_ops);
CREATE TABLE gp_float2 (c int, d real) DISTRIBUTED BY (c, d cdbhash_float4_ops);

INSERT INTO gp_float1 SELECT i, i FROM generate_series(1, 5) i;
INSERT INTO gp_float1 SELECT i, 3 FROM generate_series(1, 5) i WHERE i <> 3;
INSERT INTO gp_float2 SELECT i, i FROM generate_series(1, 5) i;

ANALYZE gp_float1;
ANALYZE gp_float2;

EXPLAIN SELECT a, b FROM gp_float1 JOIN gp_float2 ON a = c AND b = float8 '3.0';

-- Another variation: There are two constants in the same equivalence class. One's
-- datatype is compatible with the distribution key, the other's is not. We can
-- redistribute based on the compatible constant.
EXPLAIN SELECT a, b FROM gp_float1 JOIN gp_float2 ON a = c AND b = float8 '3.0' AND b = float4 '3.0';

-- Testing optimizer_enable_nljoin
SET optimizer_enable_hashjoin=off;
SET optimizer_enable_nljoin=off;
EXPLAIN SELECT * FROM t1 JOIN t2 ON t1.a=t2.a;

SET optimizer_enable_nljoin=on;
EXPLAIN SELECT * FROM t1 JOIN t2 ON t1.a=t2.a;

RESET optimizer_enable_hashjoin;
RESET optimizer_enable_nljoin;

-- Test hashed distribution spec derivation and -- 
-- motion enforcement given INDF join condition --
-- Outer joins' inner table yields false nulls  --
-- colocation if join condition is null-aware   --

--start_ignore
drop table o1;
drop table o2;
drop table o3;
--end_ignore

create table o1 (a1 int, b1 int) distributed by (a1);
create table o2 (a2 int, b2 int) distributed by (a2);
create table o3 (a3 int, b3 int) distributed by (a3);

insert into o1 select i, i from generate_series(1,20) i;
insert into o2 select i, null from generate_series(11,30) i;
insert into o3 values (NULL, 20);

select * from o1 left join o2 on a1 = a2 left join o3 on a2 is not distinct from a3;
select * from o1 left join o2 on a1 = a2 left join o3 on a2 is not distinct from a3 and b2 is distinct from b3;
select * from o1 left join o2 on a1 = a2 left join o3 on a2 is not distinct from a3 and b2 = b3;

explain select * from o1 left join o2 on a1 = a2 left join o3 on a2 is not distinct from a3;
explain select * from o1 left join o2 on a1 = a2 left join o3 on a2 is not distinct from a3 and b2 is distinct from b3;
explain select * from o1 left join o2 on a1 = a2 left join o3 on a2 is not distinct from a3 and b2 = b3;

-- Test case from community Github PR 13722
create table t_13722(id int, tt timestamp)
  distributed by (id);

-- j->jointype == join_lasj_notin
select
  t1.*
from
  t_13722 t1
where
  t1.id not in (select id from t_13722 where id != 4)
  and
  t1.tt = (select min(tt) from t_13722 where id = t1.id);

-- j->jointype == join_anti
select
  t1.*
from
  t_13722 t1
where
  not exists (select id from t_13722 where id != 4 and id = t1.id)
  and t1.tt = (select min(tt) from t_13722 where id = t1.id);

drop table t_13722;

-- This test is introduced to verify incorrect result
-- from hash join of char columns is fixed
-- Notice when varchar/text is cast to bpchar and used for
-- comparison, the trailing spaces are ignored
-- When char is cast to varchar/text, it's considered
-- comparison, and the trailing spaces are also ignored

-- Prior to the fix, opclasses belonging to different
-- opfamilies could be grouped as equivalent, and thence
-- deriving incorrect equality hash join conditions

--start_ignore
drop table foo;
drop table bar;
drop table baz;
--end_ignore
create table foo (varchar_3 varchar(3)) distributed by (varchar_3);
create table bar (char_3 char(3)) distributed by (char_3);
create table baz (text_any text) distributed by (text_any);
insert into foo values ('cd'); -- 0 trailing spaces
insert into bar values ('cd '); -- 1 trailing space
insert into baz values ('cd  '); -- 2 trailing spaces

-- varchar cast to bpchar
-- 'cd' matches 'cd', returns 1 row
explain select varchar_3, char_3 from foo join bar on varchar_3=char_3;
select varchar_3, char_3 from foo join bar on varchar_3=char_3;

-- char cast to text
-- 'cd' doesn't match 'cd  ', returns 0 rows
explain select char_3, text_any from bar join baz on char_3=text_any;
select char_3, text_any from bar join baz on char_3=text_any;

-- foo - bar join: varchar cast to bpchar
-- 'cd' matches 'cd'
-- foo - baz join: no cast
-- 'cd' doesn't match 'cd  '
-- returns 0 rows
-- Notice ORCA changes join order to minimize motion
explain select varchar_3, char_3, text_any from foo join bar on varchar_3=char_3
join baz on varchar_3=text_any;
select varchar_3, char_3, text_any from foo join bar on varchar_3=char_3
join baz on varchar_3=text_any;

-- Clean up. None of the objects we create are very interesting to keep around.
reset search_path;
set client_min_messages='warning';
drop schema bfv_joins cascade;
