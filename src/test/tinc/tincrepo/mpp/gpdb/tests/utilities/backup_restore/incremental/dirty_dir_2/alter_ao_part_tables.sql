-- @gucs gp_create_table_random_default_distribution=off
-- Alter table add default partition
Alter table ao_part05 add default partition others;

-- Alter table drop default subpartition
Alter table ao_part01 alter partition for (rank(1)) drop default partition;

-- Alter table add co partition
Alter table ao_part11 add partition p2 start(7) end(9) with(appendonly=true, orientation =column);

-- Alter table set subtransaction template
Alter table ao_part04  set subpartition template ( subpartition sp1 start (13) end (15) );

-- Alter table exchange subpartition ao with heap
create table exh_h (like ao_part02) ;
insert into exh_h values(1, '2008-02-03', 'two');
Alter table ao_part02 alter partition for (rank(2)) exchange partition for ('two') with table exh_h;

-- Alter table exchange subpartition ao with ao
create table exh_ao (like ao_part10) with(appendonly=true);
insert into exh_ao values(1, '2008-01-02', 'one');
Alter table ao_part10 alter partition for (rank(1)) exchange partition for ('one') with table exh_ao;

-- Alter table exchange subpartition ao with co
create table exh_c (like ao_part09) with(appendonly=true,orientation= column);
insert into exh_c values(1, '2008-01-02', 'one');
Alter table ao_part09 alter partition for (rank(1)) exchange partition for ('one') with table exh_c;

-- Alter table exchange default partition
create table exh_d (like ao_part08) ;
insert into exh_d values(2,27,4,6);
Alter table ao_part08 exchange default partition with table exh_d;

-- Alter table truncate default partition
Alter table ao_part06  truncate default partition;

-- Alter table truncate partiiton
Alter table ao_part03 truncate partition for (rank(1));
