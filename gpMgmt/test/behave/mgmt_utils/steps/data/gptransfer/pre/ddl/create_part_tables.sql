\c gptest;

-- @description: Heap AO and CO partition tables
-- @author: Divya Sivanandan

-- Heap part tables
--start_ignore
DROP TABLE if exists sto_heap_p1;
--end_ignore
CREATE TABLE sto_heap_p1
(
distcol int, ptcol int, subptcol int ) 
DISTRIBUTED BY (distcol) PARTITION BY range (ptcol)
SUBPARTITION BY list (subptcol) subpartition template (
default subpartition subothers, subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into sto_heap_p1 values(generate_series(1,5), generate_series(1,15), generate_series(1,7));

-- AO part tables
--start_ignore
DROP TABLE if exists sto_ao_p1;
--end_ignore
CREATE TABLE  sto_ao_p1
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))    
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into sto_ao_p1(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into sto_ao_p1(col1, col2, col3) values (2, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into sto_ao_p1(col1, col2, col3)  values(3, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into sto_ao_p1(col1, col2, col3) values (4, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into sto_ao_p1(col1, col2, col3) values (5, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');

--CO part tables
--start_ignore
Drop table if exists sto_co_p1;
--end_ignore
create table sto_co_p1(
  a int, b int, c int,d int  ) with(appendonly= true, orientation=column)    
  partition by range(b)  subpartition by range( c ) 
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )   
 ( default partition others, start(1) end(5) every(3));

Insert into sto_co_p1 values(1,generate_series(1,10),3,4);
Insert into sto_co_p1 values(2,3,generate_series(1,12),3);

-- Mixed part tables
--start_ignore
Drop table if exists sto_mx_p1;
--end_ignore
create table sto_mx_p1
(
col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
distributed randomly  partition by range(col2)
subpartition by list (col3)
subpartition template ( 
default subpartition subothers,
subpartition sub1 values ('one'), 
subpartition sub2 values ('two') with(appendonly = false),
subpartition sub3 values ('three') with(appendonly=true, orientation = column))    
(default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into sto_mx_p1(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three'), (4, '2008-01-02' ,'four');
insert into sto_mx_p1(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three'), (4, '2008-02-03' ,'four');
insert into sto_mx_p1(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two'), (4, '2008-03-04' ,'four') ;
insert into sto_mx_p1(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three'), (4, '2008-04-05', 'four');
insert into sto_mx_p1(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three'), (4, '2008-05-06', 'four');
