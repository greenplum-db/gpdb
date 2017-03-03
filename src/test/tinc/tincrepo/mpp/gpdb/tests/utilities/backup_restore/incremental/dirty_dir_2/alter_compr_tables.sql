-- @gucs gp_create_table_random_default_distribution=off
-- AO Tables

-- Alter table exchange an ao partition with co_compr table
create table exh_cc (like ao_compr03) with (appendonly=true, orientation=column, compresstype=rle_type);
insert into exh_cc values(1,2,2);
Alter table ao_compr03 alter partition for (rank(1)) exchange partition sub1 with table exh_cc;

-- Alter table split the newly added partition
 Alter table ao_compr02 split partition new_p at(17) into (partition splita , partition splitb);

--Alter table split co partition of an ao table
Alter table ao_compr04 alter partition for (rank(1)) split partition sub2 at(4) into (partition a, partition b);

-- CO Tables

-- Alter table add column with altered type
ALTER TYPE char SET DEFAULT ENCODING (compresstype=rle_type,compresslevel=4,blocksize=65536);
Alter table co_compr01 add column c2 char(3) default 'a' ;

ALTER TYPE char SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

-- Alter table add ao partition with compresstype
Alter table co_compr02 add partition new_p2 start(20) end(25) with (appendonly=true, compresstype=zlib);

-- Alter table exchange a co partition with co_compr table
create table exh_cc3 (like co_compr03) with (appendonly=true, orientation=column, compresstype=rle_type);
insert into exh_cc3 values(1,2,8);
Alter table co_compr03 alter partition for (rank(1)) exchange partition sub3 with table exh_cc3;

-- Alter table exchange a co partiiton with a ao_compr table
create table exh_ac4 (like co_compr05) with (appendonly=true, compresstype=zlib);
insert into exh_ac4 values(1,2,7,4);
Alter table co_compr05 alter partition for (rank(1)) exchange partition sub3 with table exh_ac4;

-- Alter table split newly added partition
 Alter table co_compr02 split partition new_p at(17) into (partition splita , partition splitb);


-- Alter table split ao partition of a co table
Alter table co_compr04 alter partition for (rank(1)) split partition sub1 at(2) into (partition a, partition b);

