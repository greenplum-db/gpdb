-- @gucs gp_create_table_random_default_distribution=off
-- Alter table add default partition
Alter table co_part05 add default partition others;

-- Alter table drop default subpartition
Alter table co_part01 alter partition for (rank(1)) drop default partition;

-- Alter table add co partition
Alter table co_part11 add partition p2 start(7) end(9) with(appendonly=true, orientation =column);

-- Alter table set subtransaction template
Alter table co_part04  set subpartition template ( subpartition sp1 start (13) end (15) );

-- Alter table exchange subpartition co with heap
create table exh_hp2 (like co_part02) ;
insert into exh_hp2 values(1, '2008-02-03', 'two');
Alter table co_part02 alter partition for (rank(2)) exchange partition for ('two') with table exh_hp2;

-- Alter table exchange subpartition co with ao
create table exh_ao2 (like co_part10) with(appendonly=true);
insert into exh_ao2 values(1, '2008-01-02', 'one');
Alter table co_part10 alter partition for (rank(1)) exchange partition for ('one') with table exh_ao2;

-- Alter table exchange subpartition co with co
create table exh_c2 (like co_part09) with(appendonly=true,orientation= column);
insert into exh_c2 values(1, '2008-01-02', 'one');
Alter table co_part09 alter partition for (rank(1)) exchange partition for ('one') with table exh_c2;

-- Alter table exchange default partition
create table exh_d2 (like co_part08) ;
insert into exh_d2 values(2,27,4,6);
Alter table co_part08 exchange default partition with table exh_d2;

-- Alter table truncate default partition
Alter table co_part06  truncate default partition;

-- Alter table truncate partiiton
Alter table co_part03 truncate partition for (rank(1));
