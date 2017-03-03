\c gptest;

-- start_ignore
Drop table if exists sto_altap1;
-- end_ignore

Create table sto_altap1(
  a int, b int, c int,d int  ) with(appendonly=true) partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )    ( default partition others, start(1) end(5) every(3));

Insert into sto_altap1 values(1,generate_series(1,8),3,4);
Insert into sto_altap1 values(2,3,generate_series(1,9),3);

-- start_ignore
Drop table if exists sto_altap2;
-- end_ignore

Create table sto_altap2 (a int, b int , c varchar(20)) with(appendonly=true) partition by range(a)
    (default partition others, partition p1 start(1) end(5), partition p2 start(5) end(10), partition p3 start(10) end(15));

Insert into sto_altap2 values(generate_series(1,20), 12, 'A');

-- start_ignore
Drop table if exists sto_altap3;
-- end_ignore

create table sto_altap3
 ( 
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true) 
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into sto_altap3(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into sto_altap3(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into sto_altap3(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into sto_altap3(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into sto_altap3(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');
