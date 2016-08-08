--First try AO
drop table if exists ao1;
create table ao1(a int, b int) with(appendonly=true);
insert into ao1 select i, 10 from generate_series(1, 1000) i;
update ao1 set b = b + 1;
vacuum ao1;
select * from ao1;

--Next try CO
drop table if exists co1;
create table co1(a int, b int) with(appendonly=true, orientation=column);
insert into co1 select i, 10 from generate_series(1, 1000) i;
update co1 set b = b + 1;
vacuum co1;
select * from co1;
