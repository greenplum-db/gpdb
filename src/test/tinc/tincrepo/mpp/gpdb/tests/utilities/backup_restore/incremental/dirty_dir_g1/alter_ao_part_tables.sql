-- @gucs gp_create_table_random_default_distribution=off
-- Alter table rename default partition
Alter table ao_part03 rename default partition to new_others;

-- Alter table rename partition
Alter table ao_part02 RENAME PARTITION FOR ('2008-01-01') TO jan08;

-- Alter table drop  default partition
Alter table ao_part05 drop default partition;

-- Alter table drop partition
Alter table ao_part01 drop partition for (rank(1));

-- Alter table add heap partition
Alter table ao_part04 add partition new_p start(11) end(15);

-- Alter table add ao partition
Alter table ao_part11 add partition p1 start(5) end(7) with(appendonly=true);

-- Alter table split partition
Alter table ao_part06 split partition FOR (RANK(2)) at(7) into (partition splitc,partition splitd) ;

-- Alter table split subpartition
Alter table ao_part07 alter partition p1 split partition FOR (RANK(2)) at(6) into (partition splita,partition splitb);

-- Alter table split default partition
Alter table ao_part08 split default partition start(20) end(25) into (partition p1, partition others);

-- Alter table exchange default subpartition  with ao table
create table exh_a (like ao_part09) with (appendonly=true);
insert into exh_a values(1, '2008-03-04', 'three');
Alter table ao_part09 alter partition for (rank(3)) exchange default partition with table exh_a;




