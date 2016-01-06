-- start_ignore
drop table if exists s1;
drop table if exists s2;

-- setup two partitioned tables s1 and s2
create table s1 (d1 int, p1 int)
distributed by (d1)
partition by list (p1)
(
  values (0),
  values (1));

create table s2 (d2 int, p2 int)
distributed by (d2)
partition by list (p2)
(
  values (0),
  values (1));
-- end_ignore

-- expect GPOPT fall back to legacy query optimizer 
-- since we don't support partition elimination through full outer joins
select * from s1 full outer join s2 on s1.d1 = s2.d2 and s1.p1 = s2.p2 where s1.p1 = 1;

-- start_ignore
drop table if exists s1;
drop table if exists s2;
-- end_ignore
