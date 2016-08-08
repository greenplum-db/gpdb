drop table if exists t1;
create table t1 as select * from generate_series(1, 1000) a;

drop table if exists ao_t1;
create table ao_t1 with(appendonly=true) as select * from generate_series(1, 1000) a;

drop table if exists ao_t2;
create table ao_t2 with(appendonly=true) as select * from generate_series(1, 1000) a;

drop table if exists co_t1;
create table co_t1 with(appendonly=true, orientation=column) as select * from generate_series(1, 1000) a;
