-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--- Exchange AO part with AO table
--start_ignore
Drop table if exists sto_heap_co;
--end_ignore
Create table sto_heap_co
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true, orientation=column)
 distributed randomly  partition by range(col2)
 (default partition others, 
  partition p1 start(date '2008-01-01') end(date '2008-01-30') with(appendonly=false),
  partition p2 start(date '2008-02-01') end(date '2008-02-20') with(appendonly=true, orientation=column));

insert into sto_heap_co(col1, col2, col3) values (1, '2008-01-02', 'one');
insert into sto_heap_co(col1, col2, col3) values (1, '2008-02-03', 'two');
insert into sto_heap_co(col1, col2, col3)  values(1, '2008-03-04', 'three');

select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_heap_co%');

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_heap_co');
select oid::regclass, relkind, relstorage from pg_class where relname like 'sto_heap_co%' order by oid;

--start_ignore
Drop table if exists exh_heap_co;
--end_ignore
create table exh_heap_co (like sto_heap_co) with (appendonly=true, orientation=column);
insert into exh_heap_co values (1, '2008-01-20', 'three'),  (2, '2008-01-21', 'two');
Alter table sto_heap_co exchange partition p1  with table exh_heap_co;

select oid::regclass, relkind, relstorage from pg_class where relname like 'sto_heap_co%' order by oid;

select * from sto_heap_co order by col2;

Update sto_heap_co set col4=2 where col2='2008-01-20';

set gp_select_invisible = true;
select * from sto_heap_co order by col2,col4;
set gp_select_invisible = false;
select * from sto_heap_co order by col2,col4;

Delete from sto_heap_co where col2='2008-01-21';

set gp_select_invisible = true;
select * from sto_heap_co order by col2,col4;
set gp_select_invisible = false;
select * from sto_heap_co order by col2,col4;

VACUUM sto_heap_co;

select count(*) AS only_visi_tups_vacuum  from sto_heap_co;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_heap_co;
set gp_select_invisible = false;
