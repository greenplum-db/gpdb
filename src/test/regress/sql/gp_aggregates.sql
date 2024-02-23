-- array_agg tests
SELECT array_agg(a order by a) as a_by_a from aggtest;
SELECT array_agg(b order by b) as b_by_b from aggtest;
SELECT array_agg(a order by a) as a_by_a,
       array_agg(a order by b) as a_by_b,
       array_agg(b order by a) as b_by_a,
       array_agg(b order by b) as b_by_b
  FROM aggtest;

-- Negative test cases for ordered aggregate syntax
SELECT count(order by a) from aggtest;       -- zero parameter aggregate
SELECT count(a order by a) from aggtest;     -- regular (non-orderd) aggregate
SELECT abs(a order by a) from aggtest;       -- regular function
SELECT a(aggtest order by a) from aggtest;   -- function-like column reference
SELECT nosuchagg(a order by a) FROM aggtest; -- no such function
SELECT lag(a order by a) from aggtest;       -- window function (no window clause)
SELECT lag(a order by a) over (order by a) FROM aggtest;  -- window function
SELECT count(a order by a) over (order by a) FROM aggtest;  -- window derived aggregate
SELECT array_agg(a order by a) over (order by a) FROM aggtest; -- window derived ordered aggregate

-- agg on entryDB, this used to raise error "MIN/MAX subplan has unexpected flowtype"
SELECT min(attnum) min_attnum FROM pg_catalog.pg_attribute WHERE attrelid = 'pg_proc'::regclass AND attnum > 0;

-- check for mpp-2687
CREATE TEMPORARY TABLE mpp2687t (
    dk text,
    gk text
) DISTRIBUTED BY (dk);

CREATE VIEW mpp2687v AS
    SELECT DISTINCT gk
    FROM mpp2687t
    GROUP BY gk;

SELECT * FROM mpp2687v;

-- MPP-4617
select case when ten < 5 then ten else ten * 2 end, count(distinct two), count(distinct four) from tenk1 group by 1;
select ten, ten, count(distinct two), count(distinct four) from tenk1 group by 1,2;

--MPP-20151: distinct is transformed to a group-by
select distinct two from tenk1 order by two;
select distinct two, four from tenk1 order by two, four;
select distinct two, max(two) over() from tenk1 order by two;
select distinct two, sum(four) over() from tenk1 order by two;
select distinct two, sum(four) from tenk1 group by two order by two;
select distinct two, sum(four) from tenk1 group by two having sum(four) > 5000;
select distinct t1.two, t2.two, t1.four, t2.four from tenk1 t1, tenk1 t2 where t1.hundred=t2.hundred order by t1.two, t1.four;

-- A variant with more result rows. We had a bug at one point where the
-- Motion Gather node on top of this was missing the Merge Key, and hence
-- the output came out unsorted. But it was not visible if all the rows
-- were processed on the same segment, as is the case with the above variant
-- with only two distinct 'two' values.
select distinct ten, sum(ten) over() from tenk1 order by ten;

-- Test for a planner bug we used to have, when this query gets planned
-- as a merge join. This should perform a merge join between 'l' and 'ps',
-- using both pk and sk as the merge keys. Due to the bug, the planner
-- used mix up the columns in the path keys, and used incorrect columns
-- as the merge keys. (This is a modified version of a TPC-H query)

create table l (ok bigint, pk integer, sk integer, quantity numeric) distributed by (ok);
create table ps (pk integer, sk integer, availqty integer) distributed by (pk);

insert into l select g%5, 50-g, g, 5 from generate_series(1, 50) g;
insert into ps select g, 50-g, 10 from generate_series(1, 25) g;

select  g.pk, g.sk, ps.availqty
from ps,
     (select sum(l.quantity) as qty_sum, l.pk, l.sk
      from l
      group by l.pk, l.sk ) g
where g.pk = ps.pk and g.sk = ps.sk
and ps.availqty > g.qty_sum ;

-- the same, but force a merge join and sorted agg.
set enable_hashagg=off;
set enable_hashjoin=off;
set enable_mergejoin=on;

select  g.pk, g.sk, ps.availqty
from ps,
     (select sum(l.quantity) as qty_sum, l.pk, l.sk
      from l
      group by l.pk, l.sk ) g
where g.pk = ps.pk and g.sk = ps.sk
and ps.availqty > g.qty_sum ;

reset enable_hashagg;
reset enable_hashjoin;
reset enable_mergejoin;

drop table l, ps;

-- Test having a SRF in the targetlist, with an aggregate. GPDB used to not
-- handle this, because the SRF-in-targetlist support was removed from Agg
-- node, as an optimization. It's been put back since, so this works now.
--
-- We have this same test in the upstream 'aggregates' test, but with MAX().
-- That's picked up by the MIN/MAX optimization, and turned into an
-- LIMIT 1 query, however, and doesn't exercise from the SRF-in-targetlist
-- support.
select avg(unique2), generate_series(1,3) as g from tenk1 order by g desc;


--
-- "PREFUNC" is accepted as an alias for "COMBINEFUNC", for compatibility with
-- GPDB 5 and below.
--
create function int8pl_with_notice(int8, int8) returns int8
AS $$
begin
  raise notice 'combinefunc called';
  return $1 + $2;
end;
$$ language plpgsql strict;
create aggregate mysum_prefunc(int4) (
  sfunc = int4_sum,
  stype=bigint,
  prefunc=int8pl_with_notice
);
set optimizer_force_multistage_agg = on;
select mysum_prefunc(a::int4) from aggtest;
reset optimizer_force_multistage_agg;


-- Test an aggregate with 'internal' transition type, and a combine function,
-- but no serial/deserial functions. This is valid, but we have no use for
-- the combine function in GPDB in that case.

CREATE AGGREGATE my_numeric_avg(numeric) (
  stype = internal,
  sfunc = numeric_avg_accum,
  finalfunc = numeric_avg,
  combinefunc = numeric_avg_combine
);

create temp table numerictesttab as select g::numeric as n from generate_series(1,10) g;

select my_numeric_avg(n) from numerictesttab;

--- Test distinct on UDF which EXECUTE ON ALL SEGMENTS
CREATE FUNCTION distinct_test() RETURNS SETOF boolean EXECUTE ON ALL SEGMENTS
    LANGUAGE plpgsql AS $$
BEGIN
    RETURN QUERY SELECT true;
END
$$;

SELECT DISTINCT distinct_test();

DROP FUNCTION distinct_test();

-- Test multi-phase aggregate with subquery scan
create table multiagg_with_subquery (i int, j int, k int, m int) distributed by (i);
insert into multiagg_with_subquery select i, i+1, i+2, i+3 from generate_series(1, 10)i;
explain (costs off)
select count(distinct j), count(distinct k), count(distinct m) from (select j,k,m from multiagg_with_subquery group by j,k,m ) sub group by j;
select count(distinct j), count(distinct k), count(distinct m) from (select j,k,m from multiagg_with_subquery group by j,k,m ) sub group by j;
drop table multiagg_with_subquery;

-- Test multi-phase aggregate with an expression as the group key
create table multiagg_expr_group_tbl (i int, j int) distributed by (i);
insert into multiagg_expr_group_tbl values(-1, -2), (-1, -1), (0, 1), (1, 2);
explain (costs off) select j >= 0, not j >= 0 from multiagg_expr_group_tbl group by 1;
select j >= 0, not j >= 0 from multiagg_expr_group_tbl group by 1;
select j >= 0,
		case when not j >= 0 then
			'not greater than 0'
		end
		from multiagg_expr_group_tbl group by 1;

drop table multiagg_expr_group_tbl;

CREATE TABLE multiagg_expr_group_tbl2 (
	A int,
	B int,
	C text ) DISTRIBUTED RANDOMLY;
CREATE VIEW multiagg_expr_group_view as select B, rtrim(C) AS C FROM multiagg_expr_group_tbl2;
INSERT INTO multiagg_expr_group_tbl2 VALUES(1,1,1), (2,2,2);
SELECT v1.B
  FROM multiagg_expr_group_view v1
  GROUP BY
  v1.B, v1.C
  HAVING ( v1.B= ( SELECT v2.B FROM multiagg_expr_group_view v2 WHERE v1.C = v2.C));

-- Test 3 stage aggregate with an expression contains subplan as the group key
create table agg_a (id int, a int, b int, c numeric(10, 0));
create table agg_b (id int, a int, b int, c int);
insert into agg_a values (1, 1, 1, 100);
insert into agg_b values (1, 1, 1, 1);

-- The below issue is planner only.
-- The subplan in the group key expression and the type cast "bb.v::text" under select
-- will cause the vars(b.a, b.b) showed in the expression appear in first agg's target list.
-- And grps_tlist for MppGroupContext will contains these vars with the group expression,
-- but normal case only contains group expression in grps_tlist.
-- We used to sure that there could only be one target entry(the expression here) in grps_tlist
-- for a group, but current case also contains b.a and b.b.
-- When we build the 3 stage agg and try to split the Aggref and find or add target list into
-- different Aggref stages with function `split_aggref`, it'll match to wrong Vars in
-- `AGGSTAGE_INTERMEDIATE` and `AGGSTAGE_FINAL` stage.
-- For the below query, it used to do the avg on b.b for the `AGGSTAGE_INTERMEDIATE` and
-- `AGGSTAGE_FINAL` stage and cause a crash since the type expected here should be numeric, but
-- get a int value when executing numeric_avg_deserialize.

set enable_groupagg = false;
set optimizer = off; -- the case is planner only, so disable orca
explain (costs off, verbose)
SELECT bb.v::text, count(distinct a.a), avg(a.c)	-- note the type cast
FROM agg_a a
Join ( SELECT b.b,
			(CASE WHEN b.a >= (SELECT b.b - 2)		-- note the subplan here
				THEN b.a ELSE b.b END) as v
		FROM agg_b b) as bb
ON a.a = bb.b
GROUP BY bb.v;

SELECT bb.v::text, count(distinct a.a), avg(a.c)	-- note the type cast
FROM agg_a a
Join ( SELECT b.b,
			(CASE WHEN b.a >= (SELECT b.b - 2)		-- note the subplan here
				THEN b.a ELSE b.b END) as v
		FROM agg_b b) as bb
ON a.a = bb.b
GROUP BY bb.v;

-- with multi dqa
explain (costs off, verbose)
SELECT bb.v::text, count(distinct a.a), count(distinct a.b), avg(a.c)	-- note the type cast
FROM agg_a a
Join ( SELECT b.b,
			(CASE WHEN b.a >= (SELECT b.b - 2)		-- note the subplan here
				THEN b.a ELSE b.b END) as v
		FROM agg_b b) as bb
ON a.a = bb.b
GROUP BY bb.v;

SELECT bb.v::text, count(distinct a.a), count(distinct a.b), avg(a.c)	-- note the type cast
FROM agg_a a
Join ( SELECT b.b,
			(CASE WHEN b.a >= (SELECT b.b - 2)		-- note the subplan here
				THEN b.a ELSE b.b END) as v
		FROM agg_b b) as bb
ON a.a = bb.b
GROUP BY bb.v;
reset optimizer;
reset enable_groupagg;

-- test multi DQA with guc gp_enable_mdqa_shared_scan
set optimizer = off; -- the case is planner only, so disable orca
set gp_enable_mdqa_shared_scan = true;

explain select sum(distinct a), sum(distinct b), c from agg_a group by c;
select sum(distinct a), sum(distinct b), c from agg_a group by c;

set gp_enable_mdqa_shared_scan = false;
explain select sum(distinct a), sum(distinct b), c from agg_a group by c;
select sum(distinct a), sum(distinct b), c from agg_a group by c;

reset optimizer;
reset gp_enable_mdqa_shared_scan;