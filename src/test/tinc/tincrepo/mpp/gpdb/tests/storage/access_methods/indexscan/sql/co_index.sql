drop table if exists co_t1;
drop index if exists co_i1; 
create table co_t1 with (appendonly=true, orientation=column) as select 1 as a, * from generate_series(1, 1000) as b;
create index co_i1 on co_t1(b);
select * from co_t1 order by a, b;
