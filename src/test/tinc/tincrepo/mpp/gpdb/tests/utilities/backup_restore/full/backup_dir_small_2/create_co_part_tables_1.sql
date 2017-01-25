-- @gucs gp_create_table_random_default_distribution=off
create table co_part01_1
(
distcol int, ptcol int, subptcol int ) with(appendonly=true, orientation=column)
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) subpartition template (
default subpartition subothers, subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into co_part01_1 values(generate_series(1,5), generate_series(1,15), generate_series(1,7));
