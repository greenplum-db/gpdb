--
-- Test the planner's ability to produce different kinds of plans to implement
-- grouping and aggregation.
--

drop table if exists olap_test;
drop table if exists olap_test_single;

create table olap_test(a int4, b int4, c int4, d int4) distributed by (a, b);
insert into olap_test select g / 5000, g / 1000, g / 500, g from generate_series(1, 10000) g;
analyze olap_test;

create table olap_test_single(a int4, b int4, c int4, d int4) distributed by (a);
insert into olap_test_single select g / 5000, g / 1000, g / 500, g from generate_series(1, 10000) g;
analyze olap_test_single;

-- If the GROUP BY is a superset of the table's distribution keys, the
-- aggregation can be independently in segments, and just gather the
-- results. (1-phase agg)
explain select a, b, sum(d) from olap_test group by a, b;
select a, b, sum(d) from olap_test group by a, b;

explain select a, b, c, sum(d) from olap_test group by a, b, c;
select a, b, c, sum(d) from olap_test group by a, b, c;

-- If it's not a superset, redistribution is needed.
explain select a, sum(d) from olap_test group by a;
select a, sum(d) from olap_test group by a;

-- Two-phase plain agg
explain select sum(d) from olap_test;
select sum(d) from olap_test;


--
-- DISTINCT aggregates
--

-- If the DISTINCT argument matches the distribution key, two-stage aggregation
-- is possible.

explain select sum(distinct a) from olap_test_single;
select sum(distinct a) from olap_test_single;

-- Otherwise, need a more complicated plans
explain select sum(distinct b) from olap_test_single;
select sum(distinct b) from olap_test_single;

-- If there are a lot of distinct values, then the preliminary aggregation and
-- redistribution steps are not worth the trouble, it's cheaper to just gather
-- all the input
explain select sum(distinct d) from olap_test_single;
select sum(distinct d) from olap_test_single;


--
-- GROUPING SETS
--

set gp_motion_cost_per_row=1.0;

-- If the query produces a relatively small number of groups in comparison to
-- the number of input rows, two-stage aggregation will be picked.
explain select a, b, c, sum(d) from olap_test group by grouping sets((a, b), (a), (b, c));
select a, b, c, sum(d) from olap_test group by grouping sets((a, b), (a), (b, c));

-- If the query produces a relatively large number of groups in comparison to
-- the number of input rows, one-stage aggregation will be picked.
explain select a, b, d, sum(d) from olap_test group by grouping sets((a, b), (a), (b, d));
-- do not execute this query as it would produce too many tuples.

-- Test that when the second-stage Agg doesn't try to preserve the
-- GROUPINGSET_ID(), used internally in the plan, in the result order. We had
-- a bug like that at one point.
--
-- The notable thing in the plan is that the Sort node has GROUPINGSET_ID() in
-- the Sort Key, as needed for the Finalize GroupAggregate, but in the Motion
-- above the Finalize GroupAggregate, the GROUPINGSET_ID() has been dropped
-- from the Merge Key.
set enable_hashagg=off;
explain select a, b, c, sum(d) from olap_test group by grouping sets((a, b), (a), (b, c)) limit 200;
reset enable_hashagg;

--
-- Test an optimization in the grouping planner for CREATE TABLE AS, where
-- partial aggregation results are redistributed directly according to the
-- target table's distribution keys, if they're a subset of the GROUP BY
-- columns.
--
create table foo_ctas(a int, b int) distributed randomly;
insert into foo_ctas select g%5, g%2 from generate_series(1, 100) g;
analyze foo_ctas;

explain create table bar_ctas as select * from foo_ctas group by a, b distributed by (b);
create table bar_ctas as select * from foo_ctas group by a, b distributed by (b);

-- Currently, the planner misses this optimization with INSERT, so this
-- needs an extra Redistribute Motion.
explain insert into bar_ctas select * from foo_ctas group by a, b;

drop table foo_ctas;
drop table bar_ctas;
