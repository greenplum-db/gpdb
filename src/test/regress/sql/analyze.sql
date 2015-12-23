--
-- Test null fraction
--

-- 1. empty table should have NO stats
create table analyze_test (c int) distributed by (c);
analyze analyze_test;
select stanullfrac from pg_statistic where starelid = 'analyze_test'::regclass;

-- 2. table with ALL null values, expect to see 100% null values
insert into analyze_test values(null);
analyze analyze_test;
select stanullfrac from pg_statistic where starelid = 'analyze_test'::regclass;

-- 3. table with 1 null and 1 not-null values, expect to see 50% null values
insert into analyze_test values(1);
analyze analyze_test;
select stanullfrac from pg_statistic where starelid = 'analyze_test'::regclass;

-- 4. table with ALL not-null values, expect to see 0% null values
delete from analyze_test where c is null;
analyze analyze_test;
select stanullfrac from pg_statistic where starelid = 'analyze_test'::regclass;

-- 5. column marked as not null during creation, null frac should be 0
create table analyze_test2 (a int not null) distributed by (a);
insert into analyze_test2 values(1);
analyze analyze_test2;
select stanullfrac from pg_statistic where starelid = 'analyze_test2'::regclass;

-- Clean up
drop table analyze_test;
drop table analyze_test2;

--
-- Test attribute average width
--
-- both fixed length and variable length
create table analyze_test (a int, b text) distributed by (a);

-- 1. table with all null values, column width should be 0
insert into analyze_test values(null, null);
analyze analyze_test;
select stawidth from pg_statistic where starelid = 'analyze_test'::regclass;

-- 2. regular case
insert into analyze_test select i, repeat('x',i*125) from generate_series(1, 10) i;
analyze analyze_test;
select stawidth from pg_statistic where starelid = 'analyze_test'::regclass;

-- 3. all tuples are extremely wide (width > 1024 bytes)
truncate analyze_test;
insert into analyze_test select i, repeat('x',i*1250) from generate_series(1, 10) i;
analyze analyze_test;
select stawidth from pg_statistic where starelid = 'analyze_test'::regclass;

-- Clean up
truncate table analyze_test;

--
-- Test number of distinct values
--
-- 1. all values are null, using default ndistinct -1
insert into analyze_test values(null, null);
analyze analyze_test;
select stadistinct from pg_statistic where starelid = 'analyze_test'::regclass;

-- 2. all values are unique, ndistinct should be -1
insert into analyze_test select i, i from generate_series(1,100) i;
analyze analyze_test;
select stadistinct from pg_statistic where starelid = 'analyze_test'::regclass;

-- 3. none of the values are unique, ndistinct should not extrapolate based on sample ratio
insert into analyze_test select i%101, i%101 from generate_series(1,1000) i;
analyze analyze_test;
select stadistinct from pg_statistic where starelid = 'analyze_test'::regclass;

-- 4. some values are repeated, some are not. ndistinct need to be extrapolated
insert into analyze_test select i, i from generate_series(1,1200) i;
analyze analyze_test;
select stadistinct from pg_statistic where starelid = 'analyze_test'::regclass;


