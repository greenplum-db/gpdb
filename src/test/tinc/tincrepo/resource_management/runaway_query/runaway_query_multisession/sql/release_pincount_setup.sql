drop table if exists rp;

create table rp(c1 int, c2 int);

insert into rp select i, i+1 from generate_series(1,1000)i;

analyze rp;