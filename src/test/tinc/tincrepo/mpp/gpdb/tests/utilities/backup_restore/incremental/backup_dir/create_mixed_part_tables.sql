-- @gucs gp_create_table_random_default_distribution=off
create table mixed_part01
(
distcol int, ptcol int, subptcol int ) 
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) 
subpartition template (
default subpartition subothers, 
subpartition sub1 values(1,2,3) with(appendonly=true), 
subpartition sub2 values(4,5,6) with (appendonly=true, orientation = column), 
subpartition sub3 values(7,8,9) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into mixed_part01 values(generate_series(1,5), generate_series(1,15), generate_series(1,10));

create table mixed_part02
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

insert into mixed_part02(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three'), (4, '2008-01-02' ,'four');
insert into mixed_part02(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three'), (4, '2008-02-03' ,'four');
insert into mixed_part02(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two'), (4, '2008-03-04' ,'four') ;
insert into mixed_part02(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three'), (4, '2008-04-05', 'four');
insert into mixed_part02(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three'), (4, '2008-05-06', 'four');

create table mixed_part03(
a int, b int, c int,d int  ) with(appendonly= true, orientation = column)    
partition by range(b)  subpartition by range( c ) 
subpartition template ( 
default subpartition subothers,
subpartition sub1 start (1) end(3) with(appendonly = false),
subpartition sub2 start (3) end(6) with (appendonly =true, orientation=row),
subpartition sub3 start (6) end(9))   
 ( default partition others, start(1) end(5) every(3));

Insert into mixed_part03 values(1,generate_series(1,10),3,4);
Insert into mixed_part03 values(2,3,generate_series(1,12),3);

--partition only 
create table mixed_part04( a int, b int, c int,d int  ) with(appendonly= true)
 partition by range(b) (
default partition others, 
partition p1 start(1) end(5) with(appendonly=false),
partition p2 start(5) end(10) with(appendonly=true, orientation=column),
partition p3 start(10) end(15));
 
 insert into mixed_part04 values(1, generate_series(1,20), 4, 5); 

create table mixed_part05( a int, b int, c int,d int  ) 
 partition by range(b) (
default partition others, 
partition p1 start(1) end(5) with(appendonly=true),
partition p2 start(5) end(10) with(appendonly = true, orientation=column),
partition p3 start(10) end(15));
 
 insert into mixed_part05 values(1, generate_series(1,20), 4, 5); 

create table mixed_part06( a int, b int, c int,d int  ) with(appendonly= true, orientation= column)
 partition by range(b) (
default partition others, 
partition p1 start(1) end(5) with (appendonly = false),
partition p2 start(5) end(10) with (appendonly = true),
partition p3 start(10) end(15));
 
 insert into mixed_part06 values(1, generate_series(1,20), 4, 5); 

