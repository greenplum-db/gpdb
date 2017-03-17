-- ----------------------------------------------------------------------
-- Test: DPE: Various partition selection predicates
-- ----------------------------------------------------------------------

RESET ALL;

create schema gporca_partition_selector;
set search_path to gporca_partition_selector;

-- start_ignore
create language plpythonu;
-- end_ignore

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

set optimizer=on; 
 
insert into DATE_PARTS select i, extract(year from dt), extract(month from dt), extract(day from dt), NULL from (select i, '2002-01-01'::date + i * interval '1 day' day as dt from generate_series(1, 3650) as i) as t;

-- Expected total parts => 11 * 1 * 4 => 44: 
-- TODO #141973839: we selected extra parts because of disjunction: 88 parts: 11 * 2 * 4
select part_count('explain analyze select * from DATE_PARTS where month between 1 and 3;');

-- Expected total parts => 11 * 2 * 4 => 88: 
-- TODO #141973839: we selected extra parts because of disjunction: 132 parts: 11 * 3 * 4
select part_count('explain analyze select * from DATE_PARTS where month between 1 and 4;');

-- Expected total parts => 1 * 2 * 4 => 8: 
-- TODO #141973839: we selected extra parts because of disjunction: 24 parts: 2 * 3 * 4
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
-- TODO #141973839. We expected 308 parts: (month = 1) =>   11 * 1 * 4 => 44, month > 3 => 11 * 4 * 4 => 176, month in (0, 1, 2) => 11 * 1 * 4 => 44, month is NULL => 11 * 1 * 4 => 44.
-- However, we selected 352 parts: (month = 1) =>   11 * 1 * 4 => 44, month > 3 => 11 * 4 * 4 => 176, month in (0, 1, 2) => 11 * 2 * 4 => 88, month is NULL => 11 * 1 * 4 => 44.
select part_count('explain analyze select * from DATE_PARTS where month = 1 union all select * from DATE_PARTS where month > 3 union all select * from DATE_PARTS where month in (0,1,2) union all select * from DATE_PARTS where month is null;');

-- Equality predicate
-- 44 partitions => 11 from year x 1 from month x 4 from days.
select part_count('explain analyze select * from DATE_PARTS where month = 3;');  -- Not working (it only produces general)

-- More Equality and General Predicates ---
drop table if exists foo;
create table foo(a int, b int)
partition by list (b)
(partition p1 values(1,3), partition p2 values(4,2), default partition other);

-- General predicate
-- Total 6 parts. b = 1: 1 part, b > 3: 2 parts, b in (0, 1): 2 parts. b is null: 1 part
select part_count('explain analyze select * from foo where b = 1 union all select * from foo where b > 3 union all select * from foo where b in (0,1) union all select * from foo where b is null;');

drop table if exists pt;
CREATE TABLE pt (id int, gender varchar(2)) 
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F', NULL), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

-- General filter
-- TODO #141916623. Expecting 6 parts, but optimizer plan selects 7 parts. The 6 parts breakdown is: gender = 'F': 1 part, gender < 'M': 2 parts (including default), gender in ('F', F'M'): 2 parts, gender is null => 1 part
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

-- TODO: #141973839. Expected 26 parts, currently selecting 39 parts. First level: 12 parts + 1 default. Second level 2 parts. Total 26 parts.
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
-- 8 parts: All 4 parts on first level and each will have 2 range parts 
select part_count('explain analyze select * from bar where j>0.02;');
-- 6 parts: Excluding 1 list parts at first level. So, 3 at first level and each has 2 at second level.
select part_count('explain analyze select * from bar where j>2.8;');

-- Distinct From
-- TODO: #141722947 6 parts: Everything except 1 part that contains 5.6.
select part_count('explain analyze select * from bar where j is distinct from 5.6;');
-- 8 parts: NULL is shared with others on p1. So, all 8 parts.
select part_count('explain analyze select * from bar where j is distinct from NULL;');

RESET ALL;