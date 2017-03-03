\c gptest;

-- @Description Alter exchange subparts with ao/co/heap/ao_compr/co_compr
-- @Author Divya Sivanandan

--start_ignore
Drop table if exists sto_altmsp1;
--end_ignore
create table sto_altmsp1
 (
 col1 bigint, col2 date, col3 text, col4 int) 
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( 
    default subpartition subothers, 
    subpartition sub1 values ('one', 'newone') with(appendonly=true), 
    subpartition sub2 values ('two', 'newtwo')  with(appendonly=false), 
    subpartition sub3 values('three', 'newthree') with(appendonly=true, orientation=column),
    subpartition sub4 values ('four', 'newfour') with(appendonly=true), 
    subpartition sub5 values ('five', 'newfive')  with(appendonly=false), 
    subpartition sub6 values('six', 'newsix') with(appendonly=true, orientation=column))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into sto_altmsp1(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three'), (4, '2008-01-02', 'four'), (5, '2008-01-02', 'five'), (6, '2008-01-02', 'six'), (7, '2008-01-02', 'nine');
insert into sto_altmsp1(col1, col2, col3) values (1, '2008-01-03', 'one'), (2, '2008-01-03', 'two'), (3, '2008-01-03', 'three'), (4, '2008-01-03', 'four'), (5, '2008-01-03', 'five'), (6, '2008-01-03', 'six'), (7, '2008-01-03', 'nine');

select * from sto_altmsp1 order by col1, col2;

-- Alter table exchange subpartition heap with ao_compr
--start_ignore
Drop table if exists exh_ac;
--end_ignore
create table exh_ac (like sto_altmsp1) with(appendonly=true, compresstype=zlib);
insert into exh_ac values(1, '2008-02-02', 'newtwo');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange partition for ('two') with table exh_ac;

-- Alter table exchange subpartition heap with co_compr
--start_ignore
Drop table if exists exh_cc;
--end_ignore
create table exh_cc (like sto_altmsp1) with(appendonly=true, compresstype=zlib);
insert into exh_cc values(1, '2008-02-02', 'newfive');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange partition for ('five') with table exh_cc;

-- Alter table exchange subpartition ao with co_compr
--start_ignore
Drop table if exists exh_cc1;
--end_ignore
create table exh_cc1 (like sto_altmsp1) with(appendonly=true, orientation=column, compresstype=quicklz);
insert into exh_cc1 values(1, '2008-02-02', 'newone');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange partition for ('one') with table exh_cc1;

-- Alter table exchange subpartition ao with ao_compr
--start_ignore
Drop table if exists exh_ac1;
--end_ignore
create table exh_ac1 (like sto_altmsp1) with(appendonly=true, orientation=column, compresstype=quicklz);
insert into exh_ac1 values(1, '2008-02-02', 'newfour');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange partition for ('four') with table exh_ac1;

-- Alter table exchange subpartition co  with co_compr
--start_ignore
Drop table if exists exh_cc2;
--end_ignore
create table exh_cc2 (like sto_altmsp1) with(appendonly=true,orientation= column, compresstype=zlib);
insert into exh_cc2 values(1, '2008-02-02', 'newthree');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange partition for ('three') with table exh_cc2;

-- Alter table exchange subpartition co  with ao_compr
--start_ignore
Drop table if exists exh_ac2;
--end_ignore
create table exh_ac2 (like sto_altmsp1) with(appendonly=true,orientation= column, compresstype=zlib);
insert into exh_ac2 values(1, '2008-02-02', 'newsix');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange partition for ('six') with table exh_ac2;

-- Alter table exchage default subpartition with ao table
--start_ignore
Drop table if exists exh_def;
--end_ignore
create table exh_def (like sto_altmsp1) with(appendonly=true);
insert into exh_def values(1, '2008-02-01', 'ten');
Alter table sto_altmsp1 alter partition for (rank(2)) exchange default partition with table exh_def;
