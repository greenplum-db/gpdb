-- @gucs gp_create_table_random_default_distribution=off
-- Alter table exchange heap subpartition/default  with ao
create table exh_am3 (like mixed_part01) with (appendonly=true, orientation = column);
insert into exh_am3 values(1,6,12);
Alter table mixed_part01 alter partition for (rank(2)) exchange default partition with table exh_am3;

-- Alter table exchange heap subpartition with heap
create table exh_hm3 (like mixed_part02) ;
insert into exh_hm3 values(1, '2008-02-03', 'two');
Alter table mixed_part02 alter partition for (rank(2)) exchange partition sub2 with table exh_hm3;

-- Alter table exchange co partition with ao
create table exh_cm3 (like mixed_part06) with (appendonly=true);
insert into exh_cm3 values(1,11,4,5);
Alter table mixed_part06 exchange partition p3 with table exh_cm3;

