--First try AO
drop table if exists ao1;
create table ao1(a int) with(appendonly=true);
insert into ao1 select i from generate_series(1, 1000) i;
alter table ao1 add column b int default(0);
select * from ao1;

--Next try CO
drop table if exists co1;
create table co1(a int) with(appendonly=true, orientation=column);
insert into co1 select i from generate_series(1, 1000) i;
alter table co1 add column b int default(0);
select * from co1;
