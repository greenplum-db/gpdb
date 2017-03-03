drop table if exists co_t1;
drop index if exists co_i1;
create table co_t1 (a integer, b integer) with (appendonly=true, orientation=column);
insert into co_t1 select 1 as a, * from generate_series(1, 1000) as b;
create index co_i1 on co_t1(b);
select count(*) from co_t1;

select * from readindex('co_i1'::regclass) as (ictid tid, hctid tid, aotid text, istatus text, hstatus text, val int) order by ictid;
