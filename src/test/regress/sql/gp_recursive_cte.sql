-- Tests exercising different behaviour of the WITH RECURSIVE implementation in GPDB
-- GPDB's distributed nature requires thorough testing of many use cases in order to ensure correctness

-- Setup


-- WITH RECURSIVE ref in a sublink in the main query

create schema recursive_cte;
set search_path=recursive_cte;
create table recursive_table_1(id int);
insert into recursive_table_1 values (1), (2), (100);

-- WITH RECURSIVE ref used with IN without correlation
with recursive r(i) as (
   select 1
   union all
   select i + 1 from r
)
select * from recursive_table_1 where recursive_table_1.id IN (select * from r limit 10);

-- WITH RECURSIVE ref used with NOT IN without correlation

with recursive r(i) as (
   select 1
   union all
   select i + 1 from r
)
select * from recursive_table_1 where recursive_table_1.id NOT IN (select * from r limit 10);

-- WITH RECURSIVE ref used with EXISTS without correlation

with recursive r(i) as (
   select 1
   union all
   select i + 1 from r
)
select * from recursive_table_1 where EXISTS (select * from r limit 10);

-- WITH RECURSIVE ref used with NOT EXISTS without correlation

with recursive r(i) as (
   select 1
   union all
   select i + 1 from r
)
select * from recursive_table_1 where NOT EXISTS (select * from r limit 10);

create table recursive_table_2(id int);
insert into recursive_table_2 values (11) , (21), (31);

-- WITH RECURSIVE ref used with IN & correlation
with recursive r(i) as (
	select * from recursive_table_2
	union all
	select r.i + 1 from r, recursive_table_2 where r.i = recursive_table_2.id
)
select recursive_table_1.id from recursive_table_1, recursive_table_2 where recursive_table_1.id IN (select * from r where r.i = recursive_table_2.id);

-- WITH RECURSIVE ref used with NOT IN & correlation
with recursive r(i) as (
	select * from recursive_table_2
	union all
	select r.i + 1 from r, recursive_table_2 where r.i = recursive_table_2.id
)
select recursive_table_1.id from recursive_table_1, recursive_table_2 where recursive_table_1.id NOT IN (select * from r where r.i = recursive_table_2.id);

-- WITH RECURSIVE ref used with EXISTS & correlation
with recursive r(i) as (
	select * from recursive_table_2
	union all
	select r.i + 1 from r, recursive_table_2 where r.i = recursive_table_2.id
)
select recursive_table_1.id from recursive_table_1, recursive_table_2 where recursive_table_1.id = recursive_table_2.id and EXISTS (select * from r where r.i = recursive_table_2.id);

-- WITH RECURSIVE ref used with NOT EXISTS & correlation
with recursive r(i) as (
	select * from recursive_table_2
	union all
	select r.i + 1 from r, recursive_table_2 where r.i = recursive_table_2.id
)
select recursive_table_1.id from recursive_table_1, recursive_table_2 where recursive_table_1.id = recursive_table_2.id and NOT EXISTS (select * from r where r.i = recursive_table_2.id);

-- WITH RECURSIVE ref used within a Expression sublink
with recursive r(i) as (
   select 1
   union all
   select i + 1 from r
)
select * from recursive_table_1 where recursive_table_1.id >= (select i from r limit 1) order by recursive_table_1.id;
