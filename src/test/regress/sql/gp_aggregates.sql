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
