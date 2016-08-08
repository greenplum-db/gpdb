-- @optimizer_mode on
with t (a,b,d) as (select 1,2,1 from pg_class limit 1)
SELECT cup.* FROM
 t, (
SELECT sum(t.b) OVER(PARTITION BY t.a ) AS e FROM (select 1 as a, 2 as b from pg_class limit 1)foo,t
) as cup
GROUP BY cup.e;
