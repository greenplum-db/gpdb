-- @gucs gp_create_table_random_default_distribution=off
create table ao_part01
(
distcol int, ptcol int, subptcol int ) with(appendonly=true)
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) subpartition template (
default subpartition subothers, subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into ao_part01 values(generate_series(1,5), generate_series(1,15), generate_series(1,7));

create table ao_part02
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))    
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into ao_part02(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into ao_part02(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into ao_part02(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into ao_part02(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into ao_part02(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');

create table ao_part03(
  a int, b int, c int,d int  ) with(appendonly= true)    
  partition by range(b)  subpartition by range( c ) 
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )   
 ( default partition others, start(1) end(5) every(3));

Insert into ao_part03 values(1,generate_series(1,10),3,4);
Insert into ao_part03 values(2,3,generate_series(1,12),3);

create table ao_part04(
  a int, b int, c int,d int  ) with(appendonly= true)
  partition by range(b)  subpartition by range( c )
  subpartition template ( start (1) end(13) every (7) )
 ( start(1) end(11) every(6));

insert into ao_part04 select * from ao_part03;

create table ao_part05(
  a int, b int, c int,d int  ) with(appendonly= true)
  partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )
 ( default partition others, start(1) end(5) every(3));

insert into ao_part05 select * from ao_part03;

-- Partition only table
create table ao_part06( a int, b int, c int,d int  ) with(appendonly= true)
 partition by range(b) (default partition others, start(1) end(15) every(5));

insert into ao_part06 values(1, generate_series(1,20), 4, 5);

create table ao_part07( a int, b int, c int,d int  ) with(appendonly= true)
  partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )
  (default partition others, partition p1 start(1) end(3), partition p2 start(3) end(5));

insert into ao_part07 select * from ao_part03;

-- Partition only table
create table ao_part08( a int, b int, c int,d int  ) with(appendonly= true)
 partition by range(b) (default partition others, start(1) end(15) every(5));
 
 insert into ao_part08 values(1, generate_series(1,20), 4, 5);


create table ao_part09
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into ao_part09 select * from ao_part02;

create table ao_part10
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into ao_part10 select * from ao_part02;

create table ao_part11(
  a int, b int, c int,d int  ) with(appendonly= true)
  partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers, start (1) end(7) every (4) )
 (  start(1) end(5) every(3));

Insert into ao_part11 values(1,generate_series(1,4),3,4);
Insert into ao_part11 values(2,3,generate_series(1,12),3);

create table ao_part12 ( a int, b text) with(appendonly = true) partition by range(a) (start(1) end(9) every(5));

create table ao_part13(
  a int, b int, c int,d int  ) with(appendonly= true)
  partition by range(b)  subpartition by range( c )
  subpartition template ( start (1) end(7) every (1) )
 (  start(1) end(5) every(3));

Insert into ao_part13 values(1,generate_series(1,4),3,4);
Insert into ao_part13 values(2,3,generate_series(1,6),3);

