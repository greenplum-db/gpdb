-- Testing Dynamic Partition Elimination

-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_dpe;
set search_path to qp_dpe;
SET datestyle = "ISO, DMY";
-- end_ignore

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: DPE not being applied for tables partitioned by a string column.
-- ----------------------------------------------------------------------

--
-- both sides have varchar(10)
--

-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function count_operator(explain_query text, operator text) returns int as
$$
rv = plpy.execute(explain_query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

-- ----------------------------------------------------------------------
-- Test: DPE not being applied for tables partitioned by a string column
-- ----------------------------------------------------------------------

--
-- both sides have varchar(10)
--
-- start_ignore
drop table if exists foo1;
drop table if exists foo2;

create table foo1 (i int, j varchar(10)) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j varchar(10));
insert into foo2 select i , i ||'' from generate_series(1,2) i;

analyze foo1;
analyze foo2;
-- end_ignore

select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Append') > 0;
select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Dynamic Table Scan') > 0;

select count(*) from foo1,foo2 where foo1.j = foo2.j;

--
-- both sides have text
--
-- start_ignore
drop table if exists foo1;
drop table if exists foo2;

create table foo1 (i int, j text) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j text);
insert into foo2 select i , i ||'' from generate_series(1,2) i;

analyze foo1;
analyze foo2;
-- end_ignore

select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Append') > 0;
select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Dynamic Table Scan') > 0;

select count(*) from foo1,foo2 where foo1.j = foo2.j;

--
-- partition side has text and other varchar(10)
--
-- start_ignore
drop table if exists foo1;
drop table if exists foo2;

create table foo1 (i int, j text) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j varchar(10));
insert into foo2 select i , i ||'' from generate_series(1,2) i;

analyze foo1;
analyze foo2;
-- end_ignore

select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Append') > 0;
select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Dynamic Table Scan') > 0;

select count(*) from foo1,foo2 where foo1.j = foo2.j;

--
-- partition side has varchar(10) and other side text
--
-- start_ignore
drop table if exists foo1;
drop table if exists foo2;

create table foo1 (i int, j varchar(10)) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j text);
insert into foo2 select i , i ||'' from generate_series(1,2) i;

analyze foo1;
analyze foo2;
-- end_ignore

select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Append') > 0;
select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Dynamic Table Scan') > 0;

select count(*) from foo1,foo2 where foo1.j = foo2.j;

--
-- partition side has varchar(20) and other side varchar(10)
--
-- start_ignore
drop table if exists foo1;
drop table if exists foo2;

create table foo1 (i int, j varchar(20)) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j text);
insert into foo2 select i , i ||'' from generate_series(1,2) i;

analyze foo1;
analyze foo2;
-- end_ignore

select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Append') > 0;
select count_operator('explain select count(*) from foo1,foo2 where foo1.j = foo2.j;', 'Dynamic Table Scan') > 0;

select count(*) from foo1,foo2 where foo1.j = foo2.j;

RESET ALL;

-- start_ignore
drop table if exists foo1;
drop table if exists foo2;

create table foo1 (i int, j varchar(10))
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j varchar(10));
insert into foo2 select i , i ||'' from generate_series(1,2) i;

analyze foo1;
analyze foo2;
-- end_ignore

-- ----------------------------------------------------------------------
-- Should not apply DPE when the inner side has a subplan
-- ----------------------------------------------------------------------

select count_operator('explain select count(*) from foo1,foo2 where foo1.j =foo2.j;', 'Append') > 0;
select count_operator('explain select count(*) from foo1,foo2 where foo1.j =foo2.j;', 'Dynamic Table Scan') > 0;

select count_operator('explain select count(*) from foo1,foo2 where foo1.j =foo2.j and foo2.i <= ALL(select 1 UNION select 2);', 'Dynamic Table Scan') > 0;

select count(*) from foo1,foo2 where foo1.j =foo2.j and foo2.i <= ALL(select 1 UNION select 2);

-- ----------------------------------------------------------------------
-- Test: DPE: Various partition selection predicates
-- ----------------------------------------------------------------------

-- start_ignore
create language plpythonu;
create or replace function part_count(explain_query text) returns float as
$$
import re
rv = plpy.execute(explain_query)
search_text = 'Partitions scanned'
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        p = re.compile('.+ ([\d\.]+) \(out of (\d+)\)')
        m = p.match(cur_line)
        result = result + float(m.group(1))
return result
$$
language plpythonu;
-- end_ignore

-- Check for all the different number of partition selections
DROP TABLE IF EXISTS DATE_PARTS;
CREATE TABLE DATE_PARTS (id int, year int, month int, day int, region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY LIST (month)
       SUBPARTITION TEMPLATE (
        SUBPARTITION Q1 VALUES (1, 2, 3), 
        SUBPARTITION Q2 VALUES (4 ,5 ,6),
        SUBPARTITION Q3 VALUES (7, 8, 9),
        SUBPARTITION Q4 VALUES (10, 11, 12),
        DEFAULT SUBPARTITION other_months )
        	SUBPARTITION BY RANGE(day)
        		SUBPARTITION TEMPLATE (
        		START (1) END (31) EVERY (10), 
		        DEFAULT SUBPARTITION other_days)
( START (2002) END (2012) EVERY (1), 
  DEFAULT PARTITION outlying_years );

insert into DATE_PARTS select i, extract(year from dt), extract(month from dt), extract(day from dt), NULL from (select i, '2002-01-01'::date + i * interval '1 day' day as dt from generate_series(1, 3650) as i) as t;

-- Total parts => 11 * 5 * 4 => 220

-- All parts of first level, only 2 (1 + default) from second and all 4 from third. So, total 88 parts.
select part_count('explain analyze select * from DATE_PARTS where month between 1 and 3;');

-- 11 :: 3 :: 4 => 33 x 4 => 132
select part_count('explain analyze select * from DATE_PARTS where month between 1 and 4;');

-- 2 :: 3 :: 4 => 24
select part_count('explain analyze select * from DATE_PARTS where year = 2003 and month between 1 and 4;');

-- 1 :: 5 :: 4 => 20 // Only default for year
select part_count('explain analyze select * from DATE_PARTS where year = 1999;');

-- 11 :: 1 :: 4 => 44 // Only default for month
select part_count('explain analyze select * from DATE_PARTS where month = 13;');

-- 1 :: 1 :: 4 => 4 // Default for both year and month
select part_count('explain analyze select * from DATE_PARTS where year = 1999 and month = 13;');

-- 11 :: 5 :: 1 => 55 // Only default part for day
select part_count('explain analyze select * from DATE_PARTS where day = 40;');

-- General predicate
-- Total 396: (month = 1) =>   11 * 2 * 4 => 88, month > 3 => 11 * 4 * 4 => 176, month in (0, 1, 2) => 11 * 2 * 4 => 88, month is NULL => 11 * 1 * 4 => 44
select part_count('explain analyze select * from DATE_PARTS where month = 1 union all select * from DATE_PARTS where month > 3 union all select * from DATE_PARTS where month in (0,1,2) union all select * from DATE_PARTS where month is null;');

-- Equality predicate
-- 88 partitions => 11 from year x 2 from month (including default) x 4 from days.
select part_count('explain analyze select * from DATE_PARTS where month = 3;');  -- Not working (it only produces general)

-- More Equality and General Predicates ---
drop table if exists foo;
create table foo(a int, b int)
partition by list (b)
(partition p1 values(1,3), partition p2 values(4,2), default partition other);

-- General predicate
-- Total 6 parts. b = 1: 2 parts (default included), b > 3: 2 parts, b in (0, 1): 2 parts
select part_count('explain analyze select * from foo where b = 1 union all select * from foo where b > 3 union all select * from foo where b in (0,1) union all select * from foo where b is null;');

drop table if exists pt;
CREATE TABLE pt (id int, gender varchar(2)) 
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F', NULL), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

-- General filter
-- TODO #141826453. gender = 'F': 2 parts, gender < 'M': 2 parts, gender in ('F', 'M'): 2 parts, gender is null => 2 parts. But, at present ORCA excludes default part for gender = 'F'. So, 7 parts.
select part_count('explain analyze select * from pt where gender = ''F'' union all select * from pt where gender < ''M'' union all select * from pt where gender in (''F'', ''FM'') union all select * from pt where gender is null;');

-- DML
-- Non-default part
insert into DATE_PARTS values (-1, 2004, 11, 30, NULL);
select * from date_parts_1_prt_4_2_prt_q4_3_prt_4 where id < 0;

-- Default year
insert into DATE_PARTS values (-2, 1999, 11, 30, NULL);
select * from date_parts_1_prt_outlying_years_2_prt_q4_3_prt_4 where id < 0;

-- Default month
insert into DATE_PARTS values (-3, 2004, 20, 30, NULL);
select * from date_parts_1_prt_4_2_prt_other_months where id < 0;

-- Default day
insert into DATE_PARTS values (-4, 2004, 10, 50, NULL);
select * from date_parts_1_prt_4_2_prt_q4_3_prt_other_days where id < 0;

-- Default everything
insert into DATE_PARTS values (-5, 1999, 20, 50, NULL);
select * from date_parts_1_prt_outlying_years_2_prt_other_mo_3_prt_other_days where id < 0;

-- Default month + day but not year
insert into DATE_PARTS values (-6, 2002, 20, 50, NULL);
select * from date_parts_1_prt_2_2_prt_other_months_3_prt_other_days where id < 0;

-- Dropped columns with exchange
drop table if exists sales;
CREATE TABLE sales (trans_id int, to_be_dropped1 int, date date, amount 
decimal(9,2), to_be_dropped2 int, region text) 
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), 
  SUBPARTITION asia VALUES ('asia'), 
  SUBPARTITION europe VALUES ('europe'), 
  DEFAULT SUBPARTITION other_regions)
  (START (date '2011-01-01') INCLUSIVE
   END (date '2012-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month'), 
   DEFAULT PARTITION outlying_dates );

-- This will introduce different column numbers in subsequent part tables
alter table sales drop column to_be_dropped1;
alter table sales drop column to_be_dropped2;

-- Create the exchange candidate without dropped columns
drop table if exists sales_exchange_part;
create table sales_exchange_part (trans_id int, date date, amount 
decimal(9,2), region text);

-- Insert some data
insert into sales_exchange_part values(1, '2011-01-01', 10.1, 'usa');

-- Exchange
ALTER TABLE sales 
ALTER PARTITION FOR (RANK(1))
EXCHANGE PARTITION FOR ('usa') WITH TABLE sales_exchange_part ;

-- First level: 12 parts + 1 default. Second level  2 parts?
select part_count('explain analyze select * from sales where region = ''usa'' or region = ''asia'';');
select * from sales where region = 'usa' or region = 'asia';

-- Updating partition key

update sales set region = 'europe' where trans_id = 1;
select * from sales_1_prt_2_2_prt_europe;
select * from sales_1_prt_2_2_prt_usa;
select * from sales;

-- Distinct From
drop table if exists bar;
CREATE TABLE bar (i INTEGER, j decimal)
partition by list (j)
subpartition by range (i) subpartition template (start(1) end(4) every(2))
(partition p1 values(0.2,2.8, NULL), partition p2 values(1.7,3.1),
partition p3 values(5.6), default partition other);

insert into bar values(1, 0.2); --p1
insert into bar values(1, 1.7); --p2
insert into bar values(1, 2.1); --default
insert into bar values(1, 5.6); --default
insert into bar values(1, NULL); --p1

-- In-equality
select part_count('explain analyze select * from bar where j>0.02;');
select part_count('explain analyze select * from bar where j>2.8;');

-- Distinct From
select part_count('explain analyze select * from bar where j is distinct from 5.6;');
select part_count('explain analyze select * from bar where j is distinct from NULL;');

RESET ALL;