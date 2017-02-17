drop table if exists heap_t1;
drop index if exists heap_i1;

create table heap_t1 (a integer, b integer);
insert into heap_t1 select 1 as a, * from generate_series(1, 1000) as b;
create index heap_i1 on heap_t1(b);
select count(*) from heap_t1;

select * from readindex('heap_i1'::regclass) as (ictid tid, hctid tid, aotid text, istatus text, hstatus text, val int) order by ictid;
