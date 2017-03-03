-- @gucs gp_create_table_random_default_distribution=off
create table large_part01
(
distcol int, ptcol int, subptcol int ) with(appendonly=true) 
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) 
subpartition template (
default subpartition subothers, 
subpartition sub1 values(1,2,3) with(appendonly=false), 
subpartition sub2 values(4,5,6) with (appendonly=true, orientation = column), 
subpartition sub3 values(7,8,9) with(appendonly =false),
subpartition sub4 values(10,11,12) with(appendonly=false), 
subpartition sub5 values(13,14) with (appendonly=true, orientation = column), 
subpartition sub6 values(15,16) with(appendonly =false),
subpartition sub7 values(17,18) with(appendonly=false), 
subpartition sub8 values(19,20) with (appendonly=true, orientation = column), 
subpartition sub9 values(21,22) with(appendonly =false),
subpartition sub10 values(23,24) with(appendonly=false), 
subpartition sub11 values(25,26) with (appendonly=true, orientation = column), 
subpartition sub12 values(27,28) with(appendonly =false),
subpartition sub13 values(29,30) with(appendonly=false), 
subpartition sub14 values(31,32) with (appendonly=true, orientation = column), 
subpartition sub15 values(33,34) with(appendonly =false),
subpartition sub16 values(35,36) with(appendonly=false), 
subpartition sub17 values(37,38) with (appendonly=true, orientation = column), 
subpartition sub18 values(39,40) with(appendonly =false)
)
( default partition others, start (1) end (100) inclusive every (2) );

insert into large_part01 values(generate_series(1,101), generate_series(1,45), generate_series(1,45));

Create table ao_table_t( i int, t text) with(appendonly = true);
insert into ao_table_t select i,i||'_'||repeat('ao_table',3) from generate_series(1,10)i;

Create table co_table_t( i int, t text) with(appendonly = true, orientation = column);
insert into co_table_t select i,i||'_'||repeat('co_table',3) from generate_series(1,10)i;

create table heap_table_t (i int , t text) distributed randomly;
insert into heap_table_t select i,i||'_'||repeat('heap_table',3) from generate_series(1,10)i;
