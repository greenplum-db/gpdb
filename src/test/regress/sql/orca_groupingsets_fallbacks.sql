--
-- One purpose of these tests is to make sure that ORCA can gracefully fall
-- back for these queries. To detect that, turn optimizer_trace_fallback on,
-- and watch for "falling back to planner" messages.
--
set optimizer_trace_fallback='on';

create temp table gstest1 (a int, b int, c int, d int, v int);
insert into gstest1 values (1, 5, 10, 0, 100);
insert into gstest1 values (1, 42, 20, 7, 200);
insert into gstest1 values (2, 5, 30, 21, 300);
insert into gstest1 values (2, 42, 40, 53, 400);

-- Orca falls back due to Cube
select a, b, c, sum(v) from gstest1 group by cube(a, b, c);

-- Orca falls back due to multiple grouping sets specifications
select sum(v), b, a, c from gstest1 group by c, grouping sets ((a, b), ());
select sum(v), b, a, c, d from gstest1 group by grouping sets(a, b), rollup(c, d);

-- Orca falls back due to nested grouping sets
select sum(v), b, a, c, d from gstest1 group by grouping sets(a, b, rollup(c, d));

-- Orca falls back when all grouping sets contain the primary key and the target
-- list contains a column that does not appear in any grouping set
create temp table gstest2 (a int primary key, b int, c int, d int, v int);
insert into gstest2 values (1, 1, 1, 1, 1);
insert into gstest2 values (2, 2, 2, 2, 1);
select d from gstest2 group by grouping sets ((a,b), (a));

-- Orca falls back due to HAVING clause with outer references
select v.c, (select count(*) from gstest1 group by () having v.c) from (values (false),(true)) v(c);

-- Orca falls back due to grouping function with multiple arguments
select a, b, grouping(a,b), sum(v), count(*), max(v) from gstest1 group by rollup (a,b);
-- Orca falls back due to grouping function with outer references
select (select grouping(a) from (values(1)) v2(c)) from (values(1, 2)) v1(a, b) group by (a, b);

