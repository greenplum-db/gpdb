-- @Description Checks if gp_autostats gucs are correctly handled
-- 

set gp_autostats_on_change_threshold=1;
set gp_autostats_mode=on_change;
insert into ao2 select i,'aa'||i from generate_series(21,30) as i;
select reltuples from pg_class where relname = 'ao2';
select count(*)  from ao2;
insert into ao2 select i,'aa'||i from generate_series(21,30) as i;
select count(*)  from ao2;
select reltuples from pg_class where relname = 'ao2';
insert into ao2 select i,'aa'||i from generate_series(21,30) as i;
select reltuples from pg_class where relname = 'ao2';
select count(*)  from ao2;
delete from ao2 where i < 27;
select count(*)  from ao2;
select reltuples from pg_class where relname = 'ao2';
