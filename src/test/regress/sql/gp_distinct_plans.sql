--
-- Test all the different plan shapes that the planner can generate for
-- DISTINCT queries
--
create table distinct_test (a int, b int, c int) distributed by (a);
insert into distinct_test select g / 1000, g / 2000, g from generate_series(1, 10000) g;
analyze distinct_test;

--
-- With the default cost settings, you get hashed plans
--

-- If the DISTINCT is a superset of the table's distribution keys, the
-- duplicates can be eliminated independently in the segments.
explain select distinct a, b from distinct_test;
select distinct a, b from distinct_test;

-- Otherwise, redistribution is needed
explain select distinct b from distinct_test;
select distinct b from distinct_test;

-- The two-stage aggregation can be disabled with GUC
set gp_enable_preunique = off;
explain select distinct b from distinct_test;
reset gp_enable_preunique;

-- If the input is highly unique already the pre-Unique step is not worthwhile.
-- (Only print count(*) of the result because it returns so many rows)
explain select distinct c from distinct_test;
select count(*) from (
        select distinct c from distinct_test
offset 0) as x;

--
-- Repeat the same tests with sorted Unique plans
--
set enable_hashagg=off;
set optimizer_enable_hashagg=off;

-- If the DISTINCT is a superset of the table's distribution keys, the
-- duplicates can be eliminated independently in the segments.
explain select distinct a, b from distinct_test;
select distinct a, b from distinct_test;

-- Otherwise, redistribution is needed
explain select distinct b from distinct_test;
select distinct b from distinct_test;

-- If the input is highly unique already the pre-Unique step is not worthwhile.
-- (Only print count(*) of the result because it returns so many rows)
explain select distinct c from distinct_test;
select count(*) from (
        select distinct c from distinct_test
offset 0) as x;

--
-- Also test paths where the explicit Sort is not needed
--
create index on distinct_test (a, b);
create index on distinct_test (b);
create index on distinct_test (c);
set random_page_cost=1;


-- If the DISTINCT is a superset of the table's distribution keys, the
-- duplicates can be eliminated independently in the segments.
explain select distinct a, b from distinct_test;
select distinct a, b from distinct_test;

-- Otherwise, redistribution is needed
explain select distinct b from distinct_test;
select distinct b from distinct_test;

-- If the input is highly unique already the pre-Unique step is not worthwhile.
-- (Only print count(*) of the result because it returns so many rows)
explain select distinct c from distinct_test;
select count(*) from (
        select distinct c from distinct_test
offset 0) as x;
