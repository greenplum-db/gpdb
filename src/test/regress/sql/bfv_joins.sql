--
-- Set up
--
drop table if exists x;
drop table if exists y;
drop table if exists z;
drop table if exists t;

create table x (a int, b int, c int);
insert into x values (generate_series(1,10), generate_series(1,10), generate_series(1,10));
create table y (a int, b int, c int);
insert into y (select * from x);

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

--
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
-- Clean up
--
drop table if exists x;
drop table if exists y;
drop function func_x(int);
drop table if exists z;
drop table if exists bfv_joins_foo;
drop table if exists bfv_joins_bar;
drop table if exists t;
drop table if exists x_non_part;
drop table if exists x_part;
drop function my_equality(int, int);


-- Bug-fix verification for MPP-25537: PANIC when bitmap index used in ORCA select
CREATE TABLE mpp25537_facttable1 (
  col1 integer,
  wk_id smallint,
  id integer
)
with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=5)
partition by range (wk_id) (
  start (1::smallint) END (20::smallint) inclusive every (1),
  default partition dflt
);

insert into mpp25537_facttable1 select col1, col1, col1 from (select generate_series(1,20) col1) a;

CREATE TABLE mpp25537_dimdate (
  wk_id smallint,
  col2 date
);

insert into mpp25537_dimdate select col1, current_date - col1 from (select generate_series(1,20,2) col1) a;

CREATE TABLE mpp25537_dimtabl1 (
  id integer,
  col2 integer
);

insert into mpp25537_dimtabl1 select col1, col1 from (select generate_series(1,20,3) col1) a;

CREATE INDEX idx_mpp25537_facttable1 on mpp25537_facttable1 (id);

set optimizer_analyze_root_partition to on;

ANALYZE mpp25537_facttable1;
ANALYZE mpp25537_dimdate;
ANALYZE mpp25537_dimtabl1;

SELECT count(*)
FROM mpp25537_facttable1 ft, mpp25537_dimdate dt, mpp25537_dimtabl1 dt1
WHERE ft.wk_id = dt.wk_id
AND ft.id = dt1.id;


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
