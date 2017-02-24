drop table if exists test;

create table test(c1 int, c2 int);

insert into test select i, i+1 from generate_series(1,5000)i;

analyze test;

drop role if exists rqt_temp_role;

create role rqt_temp_role;

grant all on test to rqt_temp_role;

