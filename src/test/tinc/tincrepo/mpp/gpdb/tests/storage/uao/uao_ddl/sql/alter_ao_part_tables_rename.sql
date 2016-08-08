-- @Description Alter on uao partition tables
-- 

Drop table if exists sto_alt_uao_part1;
Create table sto_alt_uao_part1(
  a int, b int, c int,d int  ) with(appendonly=true) partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )    ( default partition others, start(1) end(5) every(3));
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part1%');

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao_part1');
\d+ sto_alt_uao_part1
Insert into sto_alt_uao_part1 values(1,generate_series(1,8),3,4);
Insert into sto_alt_uao_part1 values(2,3,generate_series(1,9),3);

select count(*) from sto_alt_uao_part1;
select * from sto_alt_uao_part1;

select count(*) AS only_visi_tups from sto_alt_uao_part1;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao_part1;
set gp_select_invisible = false;


-- Alter table rename default partition
Alter table sto_alt_uao_part1 rename default partition to new_others;
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part1%');
\d+ sto_alt_uao_part1
Insert into sto_alt_uao_part1 values(1,10,3,4);
select * from sto_alt_uao_part1 order by b,c;

select count(*) AS only_visi_tups from sto_alt_uao_part1;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao_part1;
set gp_select_invisible = false;
select count(*) AS only_visi_tups from sto_alt_uao_part1;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao_part1;
set gp_select_invisible = false;

Drop table if exists sto_alt_uao_part2;
create table sto_alt_uao_part2
 ( 
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true) 
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));
\d+ sto_alt_uao_part2

select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part2%');
SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_alt_uao_part2');

insert into sto_alt_uao_part2(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into sto_alt_uao_part2(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into sto_alt_uao_part2(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into sto_alt_uao_part2(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into sto_alt_uao_part2(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');

select count(*) from sto_alt_uao_part2;
-- Alter table rename partition
Alter table sto_alt_uao_part2 RENAME PARTITION FOR ('2008-01-01') TO jan08;
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sto_alt_uao_part2%');
\d+ sto_alt_uao_part2

select count(*) AS only_visi_tups from sto_alt_uao_part2;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao_part2;
set gp_select_invisible = false;
select count(*) AS only_visi_tups from sto_alt_uao_part2;
set gp_select_invisible = true;
select count(*)  AS invisi_and_visi_tups from sto_alt_uao_part2;
set gp_select_invisible = false;
