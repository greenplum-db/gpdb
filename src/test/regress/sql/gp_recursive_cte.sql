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

-- WITH RECURSIVE ref used within an EXISTS subquery in another recursive CTE
with recursive
r(i) as (
    select 1
    union all
    select r.i + 1 from r, recursive_table_2 where i = recursive_table_2.id
),
y(i) as (
    select 1
    union all
    select i + 1 from y, recursive_table_1 where i = recursive_table_1.id and EXISTS (select * from r limit 10)
)
select * from y limit 10;

-- WITH RECURSIVE ref used within a NOT EXISTS subquery in another recursive CTE
with recursive
r(i) as (
    select 1
    union all
    select r.i + 1 from r, recursive_table_2 where i = recursive_table_2.id
),
y(i) as (
    select 1
    union all
    select i + 1 from y, recursive_table_1 where i = recursive_table_1.id and NOT EXISTS (select * from r limit 10)
)
select * from y limit 10;

-- WITH RECURSIVE ref used within an IN subquery in a non-recursive CTE
with recursive
r(i) as (
    select 1
    union all
    select r.i + 1 from r, recursive_table_2 where i = recursive_table_2.id
),
y as (
    select * from recursive_table_1 where recursive_table_1.id IN (select * from r limit 10)
)
select * from y;

-- WITH RECURSIVE ref used within a NOT IN subquery in a non-recursive CTE
with recursive
r(i) as (
    select 1
    union all
    select r.i + 1 from r, recursive_table_2 where i = recursive_table_2.id
),
y as (
    select * from recursive_table_1 where recursive_table_1.id NOT IN (select * from r limit 10)
)
select * from y;

-- WITH RECURSIVE ref used within an EXISTS subquery in a non-recursive CTE
with recursive
r(i) as (
    select 1
    union all
    select r.i + 1 from r, recursive_table_2 where i = recursive_table_2.id
),
y as (
    select * from recursive_table_1 where EXISTS (select * from r limit 10)
)
select * from y;

-- WITH RECURSIVE ref used within a NOT EXISTS subquery in a non-recursive CTE
with recursive
r(i) as (
    select 1
    union all
    select r.i + 1 from r, recursive_table_2 where i = recursive_table_2.id
),
y as (
    select * from recursive_table_1 where NOT EXISTS (select * from r limit 10)
)
select * from y;

-- WITH RECURSIVE non-recursive ref used within an EXISTS subquery in a recursive CTE
with recursive
r as (
    select * from recursive_table_2
),
y(i) as (
    select 1
    union all
    select i + 1 from y, recursive_table_1 where i = recursive_table_1.id and EXISTS (select * from r)
)
select * from y limit 10;

-- WITH RECURSIVE non-recursive ref used within a NOT EXISTS subquery in a recursive CTE
with recursive
r as (
    select * from recursive_table_2
),
y(i) as (
    select 1
    union all
    select i + 1 from y, recursive_table_1 where i = recursive_table_1.id and NOT EXISTS (select * from r)
)
select * from y limit 10;

-- WITH ref used within an IN subquery in another CTE
with
r as (
    select * from recursive_table_2 where id < 21
),
y as (
    select * from recursive_table_1 where id IN (select * from r)
)
select * from y;

-- WITH ref used within a NOT IN subquery in another CTE
with
r as (
    select * from recursive_table_2 where id < 21
),
y as (
    select * from recursive_table_1 where id NOT IN (select * from r)
)
select * from y;

-- WITH ref used within an EXISTS subquery in another CTE
with
r as (
    select * from recursive_table_2 where id < 21
),
y as (
    select * from recursive_table_1 where EXISTS (select * from r)
)
select * from y;

-- WITH ref used within a NOT EXISTS subquery in another CTE
with
r as (
    select * from recursive_table_2 where id < 21
),
y as (
    select * from recursive_table_1 where NOT EXISTS (select * from r)
)
select * from y;

create table recursive_table_3(id int, a int);
insert into recursive_table_3 values (1, 2), (2, 3);
-- WITH RECURSIVE ref used within a window function
with recursive r(i, j) as (
	select * from recursive_table_3
	union all
	select i + 1, j from r, recursive_table_3 where r.i < recursive_table_3.id
)
select avg(i) over(partition by j) from r limit 100;

-- WITH RECURSIVE ref used within a UDF
create function sum_to_zero(integer) returns bigint as $$
with recursive r(i) as (
	select $1
	union all
	select i - 1 from r where i > 0
)
select sum(i) from r;
$$ language sql;
select sum_to_zero(10);

-- WITH RECURSIVE ref used within a UDF against a distributed table
create table people(name text, parent_of text);
insert into people values ('a', 'b'), ('b', 'c'), ('c', 'd'), ('d', 'e');
create function get_lineage(text) returns setof text as $$
with recursive r(person) as (
	select name from people where name = $1
	union all
	select name from r, people where people.parent_of = r.person
)
select * from r;
$$ language sql;
select get_lineage('d');

-- non-recursive CTE nested in non-recursive enclosing CTE
INSERT INTO recursive_table_1 SELECT i FROM generate_series(0, 100) i;

SELECT MAX(j)
FROM
(
  WITH nr1(i) AS (SELECT id FROM recursive_table_1 WHERE id >= 10)
  SELECT * FROM
  (
	  WITH nr2(j) AS (SELECT i FROM nr1 WHERE i >= 50)
	  SELECT nr2.j FROM nr2, nr1
  ) sub2
) sub1;

-- non-recursive CTE nested in recursive enclosing CTE
WITH RECURSIVE r1(i) AS
(
  SELECT 1
  UNION ALL
  (
    WITH r2(j) AS
    (
      SELECT id FROM recursive_table_1 WHERE id < 5
    )
    SELECT SUM(j) FROM r2
  )
)
SELECT * FROM r1;

-- recursive CTE nested in recursive enclosing CTE
WITH RECURSIVE r1(i) AS
(
  SELECT 1
  UNION ALL
  (
    WITH RECURSIVE r2(j) AS
    (
      SELECT 1
      UNION ALL
      SELECT j + 1 FROM r2 WHERE j < 5
    ) 
    SELECT i + 1 FROM r1, r2 WHERE i < 5
  )
)
SELECT SUM(i) FROM r1;

-- recursive CTE nested in non-recursive enclosing CTE
WITH nr(i) AS
(
    WITH RECURSIVE r(j) AS
    (
      SELECT 1
      UNION ALL
      SELECT j + 1 FROM r WHERE j < 5
    ) 
    SELECT SUM(j) FROM r
)
SELECT SUM(i) FROM nr;
