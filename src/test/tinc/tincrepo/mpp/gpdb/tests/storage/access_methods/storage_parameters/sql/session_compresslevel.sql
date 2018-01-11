-- 
-- @description Guc setting at session level for compresslevel


-- Guc value to valid value compresslevel=1

\c dsp_db1

show gp_default_storage_options;

SET gp_default_storage_options="appendonly=true, compresslevel=1";
show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_ss_cl_t1;
Create table ao_ss_cl_t1 ( i int, j int);

Create index ap_t1_ix on ao_ss_cl_t1(i);

Insert into ao_ss_cl_t1 select i, i+1 from generate_series(1,10) i;

update ao_ss_cl_t1 set j=i where i>5;

Delete from ao_ss_cl_t1 where i<2;

Select count(*) from ao_ss_cl_t1;

\d+ ao_ss_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_cl_t1');

Drop table ao_ss_cl_t1;



-- Create table with compresstype=quicklz

Drop table if exists ao_ss_cl_t2;
Create table ao_ss_cl_t2 ( i int, j int) with(compresstype=quicklz);

Insert into ao_ss_cl_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t2;

\d+ ao_ss_cl_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_cl_t2');

Drop table ao_ss_cl_t2;


-- Create table with rle_type, orientation=column


Drop table if exists ao_ss_cl_t3;
Create table ao_ss_cl_t3 ( i int, j int) with(compresstype=rle_type, orientation=column);

Insert into ao_ss_cl_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t3;

\d+ ao_ss_cl_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_cl_t3');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_cl_t3'::regclass order by attnum;

Drop table ao_ss_cl_t3;


-- Create table with compresstype=zlib, Alter add column with compresslevel 9,0

Drop table if exists ao_ss_cl_t6;
Create table ao_ss_cl_t6 ( i int, j int) with(compresstype=zlib, orientation=column);

Insert into ao_ss_cl_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t6;

\d+ ao_ss_cl_t6;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t6';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_cl_t6');

Alter table ao_ss_cl_t6 add column k int default 2 encoding(compresslevel=9);
Alter table ao_ss_cl_t6 add column l int default 3 encoding(compresslevel=0);

\d+ ao_ss_cl_t6;
select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_cl_t6'::regclass order by attnum;

Drop table ao_ss_cl_t6;


-- Create table with appendonly=false

Drop table if exists ao_ss_cl_t4;
Create table ao_ss_cl_t4 ( i int, j int) with(appendonly=false);

Insert into ao_ss_cl_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t4;

\d+ ao_ss_cl_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t4';

Drop table ao_ss_cl_t4;


-- Create table with orientation and checksum

Drop table if exists ao_ss_cl_t5;
Create table ao_ss_cl_t5 ( i int, j int) with(orientation=column, checksum=true);

Insert into ao_ss_cl_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t5;

\d+ ao_ss_cl_t5

select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='ao_ss_cl_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_cl_t5');

Drop table ao_ss_cl_t5;


-- Create table with compresstype in encoding clause, add new column with new compresslevel


Drop table if exists ao_ss_cl_t7;
Create table ao_ss_cl_t7 ( i int, j int encoding(compresstype=quicklz)) with(orientation=column);

Insert into ao_ss_cl_t7 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t7;

\d+ ao_ss_cl_t7;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t7';

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_cl_t7'::regclass order by attnum;


Alter table ao_ss_cl_t7 add column k int default 3 encoding(compresstype=rle_type, compresslevel=3) ;
Alter table ao_ss_cl_t7 add column l int default 2 encoding(compresslevel=5);

show gp_default_storage_options;
SET gp_default_storage_options="appendonly=true,blocksize=8192";

show gp_default_storage_options;
Alter table ao_ss_cl_t7 add column m int default 2 encoding(compresslevel=5);

SET gp_default_storage_options="compresstype=quicklz";
Alter table ao_ss_cl_t7 add column n int default 2 encoding(compresslevel=1);

\d+ ao_ss_cl_t7;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t7';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_cl_t7');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_cl_t7'::regclass order by attnum;

Drop table ao_ss_cl_t7;


-- ========================

-- Set the sessionlevel guc to compresslevel=1 without appendonly=true
SET gp_default_storage_options="compresslevel=1";

show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_ss_cl_t1;
Create table ao_ss_cl_t1( i int, j int);

Insert into ao_ss_cl_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t1;

\d+ ao_ss_cl_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_cl_t1';

Drop table ao_ss_cl_t1;
-- with appendonly=true specified the table will be created as AO table with compresslevel

Drop table if exists ao_ss_cl_t2;
Create table ao_ss_cl_t2( i int, j int) with(appendonly=true);
Insert into ao_ss_cl_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_cl_t2;

\d+ ao_ss_cl_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_cl_t2';

Drop table ao_ss_cl_t2;

-- Set session level Guc to 2, and create compresstype=quicklz

SET gp_default_storage_options="appendonly=true, compresslevel=2";

show gp_default_storage_options;


-- Create table with compresstype=quicklz

Drop table if exists ao_ss_cl_t1;
Create table ao_ss_cl_t1 ( i int, j int) with(compresstype=quicklz);

-- Guc with compresslevel=4, Alter add column with no compresslevel for zlib
SET gp_default_storage_options="appendonly=true, compresslevel=4";
show gp_default_storage_options;

Drop table if exists ao_ss_cl_t8;
Create table ao_ss_cl_t8 ( i int, j int) with(appendonly=true, orientation=column);

\d+ ao_ss_cl_t8;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_cl_t8';

Alter table ao_ss_cl_t8 add column k int default 4 encoding(compresstype=zlib);

\d+ ao_ss_cl_t8;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_cl_t8'::regclass order by attnum;

-- with above setup add colum with compresstype quicklz

Alter table ao_ss_cl_t8 add column l int default 4 encoding(compresstype=quicklz);

Drop table ao_ss_cl_t8;

-- Reset guc to default value

RESET gp_default_storage_options;
show gp_default_storage_options;

