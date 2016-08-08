-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--- Exchange AO part with AO table
--start_ignore
Drop table if exists sto_co_heap;
--end_ignore
Create table sto_co_heap
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true, orientation=column)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into sto_co_heap(col1, col2, col3) values (1, '2008-01-02', 'one');
insert into sto_co_heap(col1, col2, col3) values (1, '2008-02-03', 'two');
insert into sto_co_heap(col1, col2, col3)  values(1, '2008-03-04', 'three');
insert into sto_co_heap(col1, col2, col3) values (1, '2008-04-05', 'one');
insert into sto_co_heap(col1, col2, col3) values (1, '2008-05-06', 'two');

select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_co_heap%');

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_co_heap');
select oid::regclass, relkind, relstorage from pg_class where relname like 'sto_co_heap_1_prt_4_2_prt_subothers%' order by oid;

--start_ignore
Drop table if exists exh_co_heap;
--end_ignore
create table exh_co_heap (like sto_co_heap);
insert into exh_co_heap values (1, '2008-03-20', 'two'),  (2, '2008-03-21', 'two');
Alter table sto_co_heap alter partition for (rank(3)) exchange partition for ('two') with table exh_co_heap;

select oid::regclass, relkind, relstorage from pg_class where relname like 'sto_co_heap_1_prt_4_2_prt_sub2%' order by oid;

select * from sto_co_heap order by col2;

Update sto_co_heap set col4=2 where col2='2008-03-20';

set gp_select_invisible = true;
select * from sto_co_heap order by col2,col4;
set gp_select_invisible = false;
select * from sto_co_heap order by col2,col4;

Delete from sto_co_heap where col2='2008-03-21';

set gp_select_invisible = true;
select * from sto_co_heap order by col2,col4;
set gp_select_invisible = false;
select * from sto_co_heap order by col2,col4;

VACUUM sto_co_heap;

select count(*) AS only_visi_tups_vacuum  from sto_co_heap;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_co_heap;
set gp_select_invisible = false;
