drop table if exists heap_t1;
drop index if exists heap_i1; 
create table heap_t1 as select 1 as a, * from generate_series(1, 1000) as b;
create index heap_i1 on heap_t1(b);
select * from heap_t1 order by a, b;
