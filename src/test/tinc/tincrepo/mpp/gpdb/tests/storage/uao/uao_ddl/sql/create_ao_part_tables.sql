-- @description : : Create Partitioned Updatable AO tables , with indexes, with and without compression
-- 

-- Create AO part tables

DROP TABLE IF EXISTS sales_uao_part cascade;
CREATE TABLE sales_uao_part (id int, date date, amt decimal(10,2)) with (appendonly=true)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION sales_Jul13 START (date '2013-07-01') INCLUSIVE ,
PARTITION sales_Aug13 START (date '2013-08-01') INCLUSIVE ,
PARTITION sales_Sep13 START (date '2013-09-01') INCLUSIVE 
END (date '2014-01-01') EXCLUSIVE );
select count(*) FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname like 'sales_uao_part%'); 

DROP TABLE if exists sto_uao_p1;
CREATE TABLE  sto_uao_p1
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two')) 
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

SELECT 1  AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_p1');
\d+ sto_uao_p1;
insert into sto_uao_p1(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into sto_uao_p1(col1, col2, col3) values (2, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into sto_uao_p1(col1, col2, col3)  values(3, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into sto_uao_p1(col1, col2, col3) values (4, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into sto_uao_p1(col1, col2, col3) values (5, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');

select count(*) from sto_uao_p1 ;
-- AO Mixed part tables
-- Mixed part tables
Drop table if exists sto_uao_mx_p1;
create table sto_uao_mx_p1
(
col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
distributed randomly  partition by range(col2)
subpartition by list (col3)
subpartition template (
default subpartition subothers,
subpartition sub1 values ('one'),
subpartition sub2 values ('two') with(appendonly = false),
subpartition sub3 values ('three') with(appendonly=true))
(default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

SELECT 1  AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='sto_uao_mx_p1');

insert into sto_uao_mx_p1(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three'), (4, '2008-01-02' ,'four');
insert into sto_uao_mx_p1(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three'), (4, '2008-02-03' ,'four');
insert into sto_uao_mx_p1(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two'), (4, '2008-03-04' ,'four') ;
insert into sto_uao_mx_p1(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three'), (4, '2008-04-05', 'four');
insert into sto_uao_mx_p1(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three'), (4, '2008-05-06', 'four');

select count(*) from sto_uao_mx_p1;
\d+ sto_uao_mx_p1;


