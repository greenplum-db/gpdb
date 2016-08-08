-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- start_ignore
drop table if exists smallt;
-- end_ignore
create table smallt (i int, t text, d date);
insert into smallt select i%10, 'text ' || (i%15), '2011-01-01'::date + ((i%20) || ' days')::interval
from generate_series(0, 99) i;

-- Rescan on Agg (with Material in the inner side of nestloop)
set enable_nestloop=off;
set enable_hashjoin=on;
select t1.*, t2.* from
(select d, count(*) from smallt group by d) as t1, (select d, sum(i) from smallt group by d) as t2
where t1.d = t2.d;
explain analyze select t1.*, t2.* from
(select d, count(*) from smallt group by d) as t1, (select d, sum(i) from smallt group by d) as t2
where t1.d = t2.d;
