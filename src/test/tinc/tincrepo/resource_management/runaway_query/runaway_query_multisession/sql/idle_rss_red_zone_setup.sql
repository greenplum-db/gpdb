drop table if exists test;

create table test(c1 int, c2 int);

insert into test select i, i+1 from generate_series(1,5000)i;

analyze test;