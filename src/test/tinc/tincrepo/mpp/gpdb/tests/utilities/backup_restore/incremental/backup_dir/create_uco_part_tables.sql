-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

create table uco_part01
(
distcol int, ptcol int, subptcol int ) with(appendonly=true, orientation=column)
distributed by (ptcol) partition by range (ptcol)
subpartition by list (subptcol) subpartition template (
default subpartition subothers, subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into uco_part01 values(generate_series(1,5), generate_series(1,15), generate_series(1,7));

create table uco_part02( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
  partition by range(b)  subpartition by range( c )
  subpartition template ( default subpartition subothers,start (1) end(7) every (4) )
  (default partition others, partition p1 start(1) end(3), partition p2 start(3) end(5));

Insert into uco_part02 values(1,generate_series(1,10),3,4);
Insert into uco_part02 values(2,3,generate_series(1,12),3);

-- Partition only table
create table uco_part03( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
 partition by range(b) (default partition others, start(1) end(15) every(5));
 
insert into uco_part03 values(1, generate_series(1,20), 4, 5);

Alter table uco_part03 add column e text default 'default';

create table uco_part04( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
 partition by range(b) (default partition others, start(1) end(15) every(5));

insert into uco_part04 values(1, generate_series(1,20), 4, 5);
Update uco_part04 set c=8;

create table uco_part05( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
 partition by range(b) (start(1) end(15) every(5));

insert into uco_part05 values(1, generate_series(1,14), 4, 5);
Update uco_part05 set c=9;

Create table uco_part08 (id int, sdate date, amt decimal(10,2)) with(appendonly=true, orientation=column)
 distributed by(id) partition by range(sdate)
    (default partition others,
     partition sales_jul13 start(date '2013-07-01') end(date '2013-07-30'),
     partition sales_aug13 start(date '2013-08-01') end(date '2013-08-30'),
     partition sales_sep13 start(date '2013-09-01') end(date '2013-09-30'));

insert into uco_part08 values (1,'2013-07-10',10.25);
insert into uco_part08 values (2,'2013-07-21',10.54);
insert into uco_part08 values (3,'2013-08-12',8.5);
insert into uco_part08 values (4,'2013-08-23',8.9);
insert into uco_part08 values (5,'2013-09-15',5.4);
insert into uco_part08 values (6,'2013-09-20',5.1);

Create table uco_part09 (id int, sdate date, amt decimal(10,2)) with(appendonly=true, orientation=column)
 distributed by(id) partition by range(sdate)
    (default partition others,
     partition sales_jul13 start(date '2013-07-01') end(date '2013-07-30'),
     partition sales_aug13 start(date '2013-08-01') end(date '2013-08-30'),
     partition sales_sep13 start(date '2013-09-01') end(date '2013-09-30'));

Insert into uco_part09 select * from uco_part08;

Create table uco_part10 (id int, sdate date, amt decimal(10,2)) with(appendonly=true, orientation=column)
 distributed by(id) partition by range(sdate)
    (default partition others,
     partition sales_jul13 start(date '2013-07-01') end(date '2013-07-30'),
     partition sales_aug13 start(date '2013-08-01') end(date '2013-08-30'),
     partition sales_sep13 start(date '2013-09-01') end(date '2013-09-30'));

Insert into uco_part10 select * from uco_part08;

Create table uco_part11 (id int, sdate date, amt decimal(10,2)) with(appendonly=true, orientation=column)
 distributed by(id) partition by range(sdate)
    (default partition others,
     partition sales_jul13 start(date '2013-07-01') end(date '2013-07-30'),
     partition sales_aug13 start(date '2013-08-01') end(date '2013-08-30'),
     partition sales_sep13 start(date '2013-09-01') end(date '2013-09-30'));

Insert into uco_part11 select * from uco_part08;

Create table uco_part12 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true, orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

insert into uco_part12 values (1,'2013-07-10',10.25,'mon');
insert into uco_part12 values (2,'2013-07-10',10.25,'thu');
insert into uco_part12 values (3,'2013-08-12',8.5,'thu');
insert into uco_part12 values (4,'2013-08-12',8.5,'wed');
insert into uco_part12 values (5,'2013-09-15',5.4, 'tue');
insert into uco_part12 values (6,'2013-09-15',5.4, 'fri');


Create table uco_part13 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true,orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

Insert into uco_part13 select * from uco_part12;

Create table uco_part14 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true,orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

Insert into uco_part14 select * from uco_part12;

Create table uco_part15 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true,orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

Insert into uco_part15 select * from uco_part12;

Create table uco_part16 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true,orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

Insert into uco_part16 select * from uco_part12;

Create table uco_part17 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true,orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

Insert into uco_part17 select * from uco_part12;

Create table uco_part18 (id int, sdate date, amt decimal(10,2), day char(3)) with(appendonly=true,orientation=column)
 distributed by(id) partition by range(sdate) subpartition by list(day)
subpartition template (default subpartition subothers , subpartition sp1 values('mon','tue','wed'), subpartition sp2 values('thu','fri','sat'))
    (default partition others,
     partition s_jul start(date '2013-07-01') end(date '2013-07-30'),
     partition s_aug start(date '2013-08-01') end(date '2013-08-30'));

Insert into uco_part18 select * from uco_part12;

-- Mixed partition table
create table uco_part06( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
partition by range(b) (
partition p1 start(1) end(5) with (appendonly = false),
partition p2 start(5) end(10) with (appendonly = true, compresstype=zlib, compresslevel=1),
partition p3 start(10) end(15) with (appendonly = true, orientation=column, compresstype=quicklz)
partition p4 start(15) end(20) with (appendonly=true, orientation=column));

insert into uco_part06 values(1, generate_series(1,19), 4, 5);
Update uco_part06 set c=7 where b=6;

create table uco_part07( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
partition by range(b) (
partition p1 start(1) end(5) with (appendonly = false),
partition p2 start(5) end(10) with (appendonly = true, compresstype=zlib, compresslevel=1),
partition p3 start(10) end(15) with (appendonly = true, orientation=column, compresstype=quicklz)
partition p4 start(15) end(20) with (appendonly=true, orientation=column));

insert into uco_part07 values(1, generate_series(1,19), 4, 5);
Delete from uco_part07 where b=16;
