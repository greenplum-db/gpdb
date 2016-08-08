drop table if exists ao_t1;
drop index if exists ao_i1; 
create table ao_t1 with (appendonly=true) as select 1 as a, * from generate_series(1, 1000) as b;
create index ao_i1 on ao_t1(b);
select * from ao_t1 order by a, b;
