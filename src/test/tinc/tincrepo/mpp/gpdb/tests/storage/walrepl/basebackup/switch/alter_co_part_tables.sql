-- @Description Alter on heap partition tables
-- 

Drop table if exists sto_altcp1;
Create table sto_altcp1(
  a int, b int, c int,d int  ) with(appendonly=true) partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )    ( default partition others, start(1) end(5) every(3));

Insert into sto_altcp1 values(1,generate_series(1,8),3,4);
Insert into sto_altcp1 values(2,3,generate_series(1,9),3);

select count(*) from sto_altcp1;

Drop table if exists sto_altcp2;
Create table sto_altcp2 (a int, b int , c varchar(20)) with(appendonly=true) partition by range(a)
    (default partition others, partition p1 start(1) end(5), partition p2 start(5) end(10), partition p3 start(10) end(15));

Insert into sto_altcp2 values(generate_series(1,20), 12, 'A');

select count(*) from sto_altcp2;

Drop table if exists sto_altcp3;
create table sto_altcp3
 ( 
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true) 
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into sto_altcp3(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into sto_altcp3(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into sto_altcp3(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into sto_altcp3(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into sto_altcp3(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');

select count(*) from sto_altcp3;


-- Alter table rename default partition
Alter table sto_altcp1 rename default partition to new_others;
Insert into sto_altcp1 values(1,10,3,4);
select * from sto_altcp1 order by b,c;

-- Alter table rename partition
Alter table sto_altcp3 RENAME PARTITION FOR ('2008-01-01') TO jan08;
select count(*) from sto_altcp3;

-- Alter table drop default partition
Alter table sto_altcp1 drop default partition;
select * from sto_altcp1 order by b,c;

-- Alter table drop partition
Alter table sto_altcp1 drop partition for (rank(1));
select * from sto_altcp1 order by b,c;

-- Alter table add heap partition
Alter table sto_altcp1 add partition new_p start(6) end(8);
Insert into sto_altcp1 values(1,7,3,4);
select * from sto_altcp1 order by b,c;

-- Alter table add ao partition
Alter table sto_altcp1 add partition p1 start(9) end(13) with(appendonly=true);
Insert into sto_altcp1 values(1,10,3,4);
select * from sto_altcp1 order by b,c;

-- Alter table add co partition
Alter table sto_altcp1 add partition p2 start(14) end(17) with(appendonly=true, orientation=column);
Insert into sto_altcp1 values(1,15,3,4);
select * from sto_altcp1 order by b,c ;

-- Alter table add default partition
Alter table sto_altcp1 add default partition part_others;
Insert into sto_altcp1 values(1,25,3,4);
select * from sto_altcp1 order by b,c;

-- Alter table split partition
Alter table sto_altcp2 split partition p1 at(3) into (partition splitc,partition splitd) ;
select * from sto_altcp2 order by a;

-- Alter table split subpartition
Alter table  sto_altcp1 alter partition p1 split partition FOR (RANK(1)) at(2) into (partition splita,partition splitb);
select * from sto_altcp1 order by b,c;

-- Alter table split default partition
Alter table sto_altcp2 split default partition start(15) end(17) into (partition p1, partition others);
select * from sto_altcp2 order by a;

