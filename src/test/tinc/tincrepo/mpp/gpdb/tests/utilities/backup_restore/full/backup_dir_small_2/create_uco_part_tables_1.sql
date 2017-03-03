-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

create table uco_part01_1
(
distcol int, ptcol int, subptcol int ) with(appendonly=true, orientation=column)
distributed by (ptcol) partition by range (ptcol)
subpartition by list (subptcol) subpartition template (
default subpartition subothers, subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into uco_part01_1 values(generate_series(1,5), generate_series(1,15), generate_series(1,7));

-- Partition only table
create table uco_part03_1( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
 partition by range(b) (default partition others, start(1) end(15) every(5));
 
insert into uco_part03_1 values(1, generate_series(1,20), 4, 5);

Alter table uco_part03_1 add column e text default 'default';

-- Mixed partition table
create table uco_part06_1( a int, b int, c int,d int  ) with(appendonly= true, orientation=column)
partition by range(b) (
partition p1 start(1) end(5) with (appendonly = false),
partition p2 start(5) end(10) with (appendonly = true, compresstype=zlib, compresslevel=1),
partition p3 start(10) end(15) with (appendonly = true, orientation=column, compresstype=quicklz)
partition p4 start(15) end(20) with (appendonly=true, orientation=column));

insert into uco_part06_1 values(1, generate_series(1,19), 4, 5);
Update uco_part06_1 set c=7 where b=6;
