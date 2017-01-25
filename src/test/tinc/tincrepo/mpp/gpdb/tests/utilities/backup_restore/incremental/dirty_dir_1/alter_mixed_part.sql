-- @gucs gp_create_table_random_default_distribution=off
-- Alter table exchange heap default partiiton with ao table
create table exh_am (like mixed_part01) with (appendonly=true);
insert into exh_am values(1,2,12);
Alter table mixed_part01 alter partition for (rank(1)) exchange default partition with table exh_am;

-- Alter table exchange ao default partition with heap table
create table exh_hm (like mixed_part02);
insert into exh_hm values(4, '2008-01-02' ,'four');
Alter table mixed_part02 alter partition for (rank(1)) exchange default partition with table exh_hm;

-- Alter table exchange co default partition wth heap table
create table exh_hm2 (like mixed_part03);
insert into exh_hm2 values(2,3,10);
Alter table mixed_part03 alter partition for (rank(1)) exchange default partition with table exh_hm2;

-- Alter table exchange heap partition with ao table
create table exh_am2 (like mixed_part04) with(appendonly=true);
insert into exh_am2 values(1,3,4,5);
Alter table mixed_part04 exchange partition p1 with table exh_am2;

-- Alter table exchange ao partition with co table
create table exh_cm (like mixed_part05) with (appendonly=true, orientation=column);
insert into exh_cm values(1,3,4,5);
Alter table mixed_part05 exchange partition p1 with table exh_cm;


