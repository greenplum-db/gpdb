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

-- If the query produces a relatively small number of groups in comparison to
-- the number of input rows, two-stage aggregation will be picked.
explain select a, b, c, sum(d) from olap_test group by grouping sets((a, b), (a), (b, c));
select a, b, c, sum(d) from olap_test group by grouping sets((a, b), (a), (b, c));

-- If the query produces a relatively large number of groups in comparison to
-- the number of input rows, one-stage aggregation will be picked.
explain select a, b, d, sum(d) from olap_test group by grouping sets((a, b), (a), (b, d));
-- do not execute this query as it would produce too many tuples.
