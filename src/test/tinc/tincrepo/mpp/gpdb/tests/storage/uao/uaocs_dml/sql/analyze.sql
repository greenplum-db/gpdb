-- @Description Checks analyze behavior w.r.t. to statistics
-- 

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
analyze ao2;
select count(*)  from ao2;
select reltuples from pg_class where relname = 'ao2';
