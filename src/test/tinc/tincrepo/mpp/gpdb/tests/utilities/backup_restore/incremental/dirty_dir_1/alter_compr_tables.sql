-- @gucs gp_create_table_random_default_distribution=off
-- AO table

-- Alter table add column
Alter table ao_compr01 ADD COLUMN added_col character varying(30) default 'default';

-- Alter AO part table add new ao part with compression
Alter table ao_compr02 add partition new_p start(15) end(20) with (appendonly=true, compresstype=zlib);

-- Alter table exchange a heap partition with ao_compr table
create table exh_ac (like ao_compr03) with (appendonly=true, compresstype=zlib);
insert into exh_ac values(1,2,12);
Alter table ao_compr03 alter partition for (rank(1)) exchange default partition with table exh_ac;

-- Alter table exchange an ao partiiton with a ao_compr table
create table exh_ac2 (like ao_compr05) with (appendonly=true, compresstype=zlib);
insert into exh_ac2 values(1,2,5,4);
Alter table ao_compr05 alter partition for (rank(1)) exchange partition sub2 with table exh_ac2;


-- CO table

-- Alter table add column with encoding
Alter table co_compr01 add column c1 int default 1 encoding (compresstype=rle_type);

-- Alter table add partition with compresstype
Alter table co_compr02 add partition new_p start(15) end(20) with (appendonly=true, orientation = column, compresstype=rle_type);

-- Alter table exchange a heap partition with co_compr table
create table exh_cc2 (like co_compr03) with (appendonly=true,orientation = column, compresstype=zlib);
insert into exh_cc2 values(1,2,12);
Alter table co_compr03 alter partition for (rank(1)) exchange default partition with table exh_cc2;

-- Alter table exchange a co partiiton with a ao_compr table
create table exh_ac3 (like co_compr05) with (appendonly=true, compresstype=zlib);
insert into exh_ac3 values(1,2,7,4);
Alter table co_compr05 alter partition for (rank(1)) exchange partition sub3 with table exh_ac3;
