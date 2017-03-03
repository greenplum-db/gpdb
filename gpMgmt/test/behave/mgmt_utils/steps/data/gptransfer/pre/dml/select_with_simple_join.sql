\c gptest;

-- start_ignore
drop table if exists tt1 cascade;
drop table if exists tt2 cascade;
-- end_ignore

create table tt1(x int) distributed by (x);
create table tt2(x int, y int, z int) distributed by (y);
create index tt1i on tt1 using btree(x);

insert into tt1 select generate_series(1,1000);
insert into tt2 select i, i%2, i%3 from generate_series(1,1000) i;

