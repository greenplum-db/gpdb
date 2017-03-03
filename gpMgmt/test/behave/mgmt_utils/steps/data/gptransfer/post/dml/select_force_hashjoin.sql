\c gptest;

-- Rescan on Agg (with Material in the inner side of nestloop)
set enable_nestloop=off;
set enable_hashjoin=on;
select t1.*, t2.* from
(select d, count(*) from smallt group by d) as t1, (select d, sum(i) from smallt group by d) as t2
where t1.d = t2.d;
