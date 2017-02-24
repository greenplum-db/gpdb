drop table if exists ao_t1;
drop index if exists ao_i1;
create table ao_t1 (a integer, b integer) with (appendonly=true);
insert into ao_t1 select 1 as a, * from generate_series(1, 1000) as b;
create index ao_i1 on ao_t1(b);
select count(*) from ao_t1;

select * from readindex('ao_i1'::regclass) as (ictid tid, hctid tid, aotid text, istatus text, hstatus text, val int) order by ictid;
