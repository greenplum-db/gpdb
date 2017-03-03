-- @gucs gp_create_table_random_default_distribution=off
create table mixed_part01_1
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

insert into mixed_part01_1 values(generate_series(1,5), generate_series(1,15), generate_series(1,10));

--partition only 
create table mixed_part04_1( a int, b int, c int,d int  ) with(appendonly= true)
 partition by range(b) (
default partition others, 
partition p1 start(1) end(5) with(appendonly=false),
partition p2 start(5) end(10) with(appendonly=true, orientation=column),
partition p3 start(10) end(15));
 
insert into mixed_part04_1 values(1, generate_series(1,20), 4, 5); 
