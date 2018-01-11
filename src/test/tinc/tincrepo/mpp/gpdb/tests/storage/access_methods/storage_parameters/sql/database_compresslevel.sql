-- 
-- @description Guc setting at database level for compresslevel


-- Guc value to valid value compresslevel=1
Alter database dsp_db1 set gp_default_storage_options="appendonly=true, compresslevel=1";

Select datconfig from pg_database where datname='dsp_db1';

\c dsp_db1

show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_db_cl_t1;
Create table ao_db_cl_t1 ( i int, j int);

Create index ap_t1_ix on ao_db_cl_t1(i);

Insert into ao_db_cl_t1 select i, i+1 from generate_series(1,10) i;

update ao_db_cl_t1 set j=i where i>5;

Delete from ao_db_cl_t1 where i<2;

Select count(*) from ao_db_cl_t1;

\d+ ao_db_cl_t1;

select relstorage, reloptions from pg_class where relname='ao_db_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t1');

Alter table ao_db_cl_t1 add column k int default 2;

\d+ ao_db_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t1');


SET gp_default_storage_options='appendonly=true, orientation=column, blocksize=8192,compresslevel=0';

Alter table ao_db_cl_t1 add column l int default 3;

\d+ ao_db_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t1');

SET gp_default_storage_options='appendonly=false,compresstype=quicklz';

Alter table ao_db_cl_t1 add column m int default 5;

\d+ ao_db_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t1');


RESET gp_default_storage_options;
show gp_default_storage_options;

Drop table ao_db_cl_t1;



-- Create table with compresstype=quicklz

Drop table if exists ao_db_cl_t2;
Create table ao_db_cl_t2 ( i int, j int) with(compresstype=quicklz);

Insert into ao_db_cl_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t2;

\d+ ao_db_cl_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t2');

Drop table ao_db_cl_t2;


-- Create table with rle_type, orientation=row/column

Drop table if exists ao_db_cl_t3;
Create table ao_db_cl_t3 ( i int, j int) with(compresstype=rle_type);

Drop table if exists ao_db_cl_t3;
Create table ao_db_cl_t3 ( i int, j int) with(compresstype=rle_type, orientation=column);

Insert into ao_db_cl_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t3;

\d+ ao_db_cl_t3;

select relstorage, reloptions from pg_class where relname='ao_db_cl_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t3');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_db_cl_t3'::regclass order by attnum;

Drop table ao_db_cl_t3;


-- Create table with compresstype=zlib

Drop table if exists ao_db_cl_t6;
Create table ao_db_cl_t6 ( i int, j int) with(compresstype=zlib);

Insert into ao_db_cl_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t6;

\d+ ao_db_cl_t6;

select relstorage, reloptions from pg_class where relname='ao_db_cl_t6';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t6');

Drop table ao_db_cl_t6;


-- Create table with appendonly=false

Drop table if exists ao_db_cl_t4;
Create table ao_db_cl_t4 ( i int, j int) with(appendonly=false);

Insert into ao_db_cl_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t4;

\d+ ao_db_cl_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t4';

Drop table ao_db_cl_t4;


-- Create table with orientation and checksum

Drop table if exists ao_db_cl_t5;
Create table ao_db_cl_t5 ( i int, j int) with(orientation=column, checksum=true);

Insert into ao_db_cl_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t5;

\d+ ao_db_cl_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t6';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t6');

Drop table ao_db_cl_t5;


-- Create table with compresstype in encoding clause, add new column with new compresslevel


Drop table if exists ao_db_cl_t7;
Create table ao_db_cl_t7 ( i int, j int encoding(compresstype=quicklz)) with(orientation=column);

Insert into ao_db_cl_t7 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t7;

\d+ ao_db_cl_t7;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t7';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t7');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_db_cl_t7'::regclass order by attnum;


Alter table ao_db_cl_t7 add column k int default 3 encoding(compresstype=rle_type, compresslevel=3) ;
Alter table ao_db_cl_t7 add column l int default 2 encoding(compresslevel=5);

\d+ ao_db_cl_t7;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_cl_t7';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t7');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_db_cl_t7'::regclass order by attnum;

Drop table ao_db_cl_t7;


-- ========================

-- Set database level Guc to 2, and create compresstype=quicklz

Alter database dsp_db2 set gp_default_storage_options="appendonly=true, compresslevel=2";

Select datconfig from pg_database where datname='dsp_db2';

\c dsp_db2

show gp_default_storage_options;


-- Create table with compresstype=quicklz

Drop table if exists ao_db_cl_t1;
Create table ao_db_cl_t1 ( i int, j int) with(compresstype=quicklz);

-- Guc with no appendonly=true specified
Alter database dsp_db4 set gp_default_storage_options="compresslevel=1";

\c dsp_db4
show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_db_cl_t1;
Create table ao_db_cl_t1( i int, j int);

Insert into ao_db_cl_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t1;

\d+ ao_db_cl_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_cl_t1';

Drop table ao_db_cl_t1;
-- with appendonly=true specified the table will be created as AO table with compresslevel

Drop table if exists ao_db_cl_t2;
Create table ao_db_cl_t2( i int, j int) with(appendonly=true);
Insert into ao_db_cl_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_cl_t2;

\d+ ao_db_cl_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_cl_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_cl_t2');

Drop table ao_db_cl_t2;


-- Reset guc to default value for all four databases

Alter database dsp_db1 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db2 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db3 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db4 set gp_default_storage_options TO DEFAULT;

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
