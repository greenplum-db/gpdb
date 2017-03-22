-- 
-- @description Guc setting at database level for compresstype


-- Guc value compresstype=zlib
Alter database dsp_db1 set gp_default_storage_options="appendonly=true, compresstype=zlib";

Select datconfig from pg_database where datname='dsp_db1';

\c dsp_db1

show gp_default_storage_options;

-- Create able with no options
Drop table if exists ao_db_ct_t1;
Create table ao_db_ct_t1 ( i int, j int);

Create index ap_t1_ix on ao_db_ct_t1(i);

Insert into ao_db_ct_t1 select i, i+1 from generate_series(1,10) i;

update ao_db_ct_t1 set j=i where i>5;

Delete from ao_db_ct_t1 where i<2;

Select count(*) from ao_db_ct_t1;

\d+ ao_db_ct_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ct_t1');

Drop table ao_db_ct_t1;



-- Create table with compresstype=quicklz

Drop table if exists ao_db_ct_t2;
Create table ao_db_ct_t2 ( i int, j int) with(compresstype=quicklz);

Insert into ao_db_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t2;

\d+ ao_db_ct_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ct_t2');

Drop table ao_db_ct_t2;



-- Create table with compresstype=rle_type(not valid)

Drop table if exists ao_db_ct_t3;
Create table ao_db_ct_t3 ( i int, j int) with(compresstype=rle_type);


-- Create table with compresstype=none

Drop table if exists ao_db_ct_t4;
Create table ao_db_ct_t4 ( i int, j int) with(compresstype=none);

Insert into ao_db_ct_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t4;

\d+ ao_db_ct_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t4';


-- Guc value compresstype=zlib, orientation=column
Alter database dsp_db2 set gp_default_storage_options="appendonly=true, orientation=column, compresstype=zlib";

Select datconfig from pg_database where datname='dsp_db2';

\c dsp_db2

show gp_default_storage_options;

Drop table if exists ao_db_ct_t4;
Create table ao_db_ct_t4 ( i int, j int) with(compresstype=none);

-- Alter the table and add a column with new compresstype
Alter table ao_db_ct_t4 add column k int default 10 ENCODING (compresstype=quicklz);

\d+ ao_db_ct_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t4';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ct_t4');

Drop table ao_db_ct_t4;


-- Create table with compresstype=rle_type  and orientation=column

Drop table if exists ao_db_ct_t3;
Create table ao_db_ct_t3 ( i int, j int) with(compresstype=rle_type, orientation=column);


Insert into ao_db_ct_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t3;

\d+ ao_db_ct_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ct_t3');

Drop table ao_db_ct_t3;

-- Create table with a new compresstype at column level for one column 

Drop table if exists ao_db_ct_t5;
Create table ao_db_ct_t5 ( i int, j int encoding(compresstype=quicklz));

Insert into ao_db_ct_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t5;

\d+ ao_db_ct_t5;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_db_ct_t5'::regclass order by attnum;

Drop table ao_db_ct_t5;

-- Create table with compresstype in default column encoding

Drop table if exists ao_db_ct_t6;
Create table ao_db_ct_t6 ( i int, j int, DEFAULT COLUMN ENCODING (compresstype=quicklz));

Insert into ao_db_ct_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t6;

\d+ ao_db_ct_t6;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_db_ct_t6'::regclass order by attnum;

Drop table ao_db_ct_t6;
-- Create table with invalid value for compresstype

Drop table if exists ao_db_ct_t4;
Create table ao_db_ct_t4 ( i int, j int) with(compresstype=xxxx);



-- Create table with orientation,checksum,compresslevel

Drop table if exists ao_db_ct_t5;
Create table ao_db_ct_t5 ( i int, j int) with(orientation=column, compresslevel=1, checksum=true);

Insert into ao_db_ct_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t5;

\d+ ao_db_ct_t5

select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='ao_db_ct_t5';

Drop table ao_db_ct_t5;


-- ========================================
-- Set the database level guc to quicklz
show gp_default_storage_options;

Alter database dsp_db3 set gp_default_storage_options="appendonly=true,compresstype=quicklz";

Select datconfig from pg_database where datname='dsp_db3';

\c dsp_db3

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_db_ct_t1;
Create table ao_db_ct_t1 ( i int, j int);

Insert into ao_db_ct_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_db_ct_t1;

\d+ ao_db_ct_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t1';

Drop table ao_db_ct_t1;

-- Set the database level guc to rle_type


Alter database dsp_db3 set gp_default_storage_options="appendonly=true, compresstype=rle_type, orientation=column";

Select datconfig from pg_database where datname='dsp_db3';

\c dsp_db3

show gp_default_storage_options;

-- Create table with no options 

Drop table if exists ao_db_ct_t2;
Create table ao_db_ct_t2 ( i int, j int);

Insert into ao_db_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t2;

\d+ ao_db_ct_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ct_t2');

Drop table ao_db_ct_t2;


-- Create table with appendonly=false

Drop table if exists ao_db_ct_t3;
Create table ao_db_ct_t3 ( i int, j int) with(appendonly=false);

Insert into ao_db_ct_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t3;

\d+ ao_db_ct_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ct_t3';

Drop table ao_db_ct_t3;


-- Create table with appendonly=true

Drop table if exists ao_db_ct_t4;
Create table ao_db_ct_t4 ( i int, j int) with(appendonly=true);



--- =====================================
-- GUC with invalid value, invalid combinations
Alter database dsp_db1 set gp_default_storage_options="appendonly=true,compresstype=xxx";
Alter database dsp_db1 set gp_default_storage_options="appendonly=true, compresstype=rle_type";
Alter database dsp_db1 set gp_default_storage_options="appendonly=true, compresstype=quicklz,compresslevel=4";

-- Guc with no appendonly=true specified
Alter database dsp_db4 set gp_default_storage_options="compresstype=zlib";

\c dsp_db4
show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_db_ct_t1;
Create table ao_db_ct_t1( i int, j int);

Insert into ao_db_ct_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t1;

\d+ ao_db_ct_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_ct_t1';

Drop table ao_db_ct_t1;
-- with appendonly=true specified the table will be created as AO table with compresstype

Drop table if exists ao_db_ct_t2;
Create table ao_db_ct_t2( i int, j int) with(appendonly=true);
Insert into ao_db_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ct_t2;

\d+ ao_db_ct_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_ct_t2';

Drop table ao_db_ct_t2;
-- Reset guc to default value for all three databases

Alter database dsp_db1 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db2 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db3 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db4 set gp_default_storage_options TO DEFAULT;

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
