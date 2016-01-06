--
-- Setup
--
set default_statistics_target=25;
set allow_system_table_mods="DML";
set time zone 'Europe/Rome';

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

-- Clean up
truncate table analyze_test;

--
-- Test most common values(MCV), most common frequencies(MCF) and histogram
--
-- 1. stats are not collected if the column is all null values
insert into analyze_test select null, null from generate_series(1, 51) i;
analyze analyze_test;
select staattnum, stadistinct, stanumbers1, stavalues1, stavalues2 from pg_statistic where starelid = 'analyze_test'::regclass;

-- 2. stats are not collected if the column is too wide
truncate table analyze_test;
insert into analyze_test select i, repeat('x',1025) from generate_series(1, 51) i;
analyze analyze_test;
select staattnum, stadistinct, stanumbers1, stavalues1, stavalues2 from pg_statistic where starelid = 'analyze_test'::regclass;

-- 3. for boolean type we only need to collect MCV
create table analyze_test_bool (c boolean) distributed by (c);
insert into analyze_test_bool select i%2=0 from generate_series(1,55) i;
analyze analyze_test_bool;
select staattnum, stadistinct, stanumbers1, stavalues1, stavalues2 from pg_statistic where starelid = 'analyze_test_bool'::regclass;

-- 4. histogram will be empty if bucket_size <= 1
truncate table analyze_test;
insert into analyze_test select i, repeat('x',i%5) from generate_series(1, 10) i;
analyze analyze_test;
select staattnum, stadistinct, stanumbers1, stavalues1, stavalues2 from pg_statistic where starelid = 'analyze_test'::regclass;
insert into analyze_test select i, repeat('x',i%5) from generate_series(11, 25) i;
analyze analyze_test;
select staattnum, stadistinct, stanumbers1, stavalues1, stavalues2 from pg_statistic where starelid = 'analyze_test'::regclass;

-- 5. Normal case
insert into analyze_test select i, repeat('x',i%25) from generate_series(1, 40) i;
analyze analyze_test;
select staattnum, stadistinct, stanumbers1, stavalues1, stavalues2 from pg_statistic where starelid = 'analyze_test'::regclass;

--
-- Test with multiple columns, with different stats target
--
create table analyze_cols (
  a int, b text, c bigint,
  d double precision, e boolean,
  f char(50), g varchar(300),
  h date, i decimal(12,2), j real,
  k time, l timetz,
  m timestamp, n timestamptz
) distributed by (a);

update pg_attribute set attstattarget=0 where attrelid='analyze_cols'::regclass and attname='c';
update pg_attribute set attstattarget=1 where attrelid='analyze_cols'::regclass and attname='f';
update pg_attribute set attstattarget=2 where attrelid='analyze_cols'::regclass and attname='i';
update pg_attribute set attstattarget=51 where attrelid='analyze_cols'::regclass and attname='j';
update pg_attribute set attstattarget=70 where attrelid='analyze_cols'::regclass and attname='h';
update pg_attribute set attstattarget=100 where attrelid='analyze_cols'::regclass and attname='d';

insert into analyze_cols
  select x,
  repeat('abcdef',(x%73)*20),
  (x%71)*12345670,
  x*321.456,
  x%3=0,
  repeat('yz',x/4),
  repeat('abcd',x%73),
  date '2016-01-12' + (x%73)*10,
  7123.45*(x%19),
  123.456*(x%51),
  (x%24||':'||x%19)::time,
  (x%24||':'||x%19||':00+08')::timetz,
  ('2016-01-12 '||x%24||':'||x%59)::timestamp,
  ('2016-01-12 '||x%24||':'||x%25||':00+09')::timestamptz
  from generate_series(1, 100) x;

analyze analyze_cols;
select * from pg_stats where schemaname='public' and tablename='analyze_cols';

--
-- Clean up
--
drop table analyze_test_bool;
drop table analyze_test;
drop table analyze_cols;
set time zone default;
set allow_system_table_mods="NONE";
reset default_statistics_target;
