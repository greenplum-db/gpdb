-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- start_ignore
drop table if exists tt1 cascade;
drop table if exists tt2 cascade;
-- end_ignore

create table tt1(x int) distributed by (x);
create table tt2(x int, y int, z int) distributed by (y);
create index tt1i on tt1 using btree(x);

insert into tt1 select generate_series(1,1000);
insert into tt2 select i, i%2, i%3 from generate_series(1,1000) i;


set enable_seqscan=off;
set enable_mergejoin=on;
set enable_nestloop=off;
set enable_hashjoin=off;
set enable_bitmapscan=off;
set enable_indexscan=on;

--allow catalog modification
set allow_system_table_mods=true;
update pg_class set reltuples=1 where relname='tt2';

select tt1.x from tt2 full outer join tt1 on (tt1.x=tt2.z) where tt2.z=2;
