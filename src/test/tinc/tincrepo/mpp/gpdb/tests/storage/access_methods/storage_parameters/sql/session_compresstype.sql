-- 
-- @description Guc setting at session level for compresstype


-- Guc value compresstype=zlib

\c dsp_db1

show gp_default_storage_options;

SET gp_default_storage_options="appendonly=true, compresstype=zlib";
show gp_default_storage_options;

-- Create able with no options
Drop table if exists ao_ss_ct_t1;
Create table ao_ss_ct_t1 ( i int, j int);

Create index ap_t1_ix on ao_ss_ct_t1(i);

Insert into ao_ss_ct_t1 select i, i+1 from generate_series(1,10) i;

update ao_ss_ct_t1 set j=i where i>5;

Delete from ao_ss_ct_t1 where i<2;

Select count(*) from ao_ss_ct_t1;

\d+ ao_ss_ct_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t1');

Drop table ao_ss_ct_t1;



-- Create table with compresstype=quicklz

Drop table if exists ao_ss_ct_t2;
Create table ao_ss_ct_t2 ( i int, j int) with(compresstype=quicklz);

Insert into ao_ss_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t2;

\d+ ao_ss_ct_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t2');

Drop table ao_ss_ct_t2;



-- Create table with compresstype=rle_type(not valid)

Drop table if exists ao_ss_ct_t3;
Create table ao_ss_ct_t3 ( i int, j int) with(compresstype=rle_type);


-- Create table with compresstype=none

Drop table if exists ao_ss_ct_t4;
Create table ao_ss_ct_t4 ( i int, j int) with(compresstype=none);

Insert into ao_ss_ct_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t4;

\d+ ao_ss_ct_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t4';

SET gp_default_storage_options="appendonly=true,blocksize=8192";
show gp_default_storage_options;

Alter table ao_ss_ct_t4 add column k int default 20;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t4';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t4');

SET gp_default_storage_options="appendonly=true, orientation=column, compresstype=zlib";
show gp_default_storage_options;

Alter table ao_ss_ct_t4 add column l int default 10;

-- Alter the table and add a column with new compresstype
Alter table ao_ss_ct_t4 add column m int default 10 ENCODING (compresstype=quicklz);

\d+ ao_ss_ct_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t4';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t4');

Drop table ao_ss_ct_t4;


-- Create table with compresstype=rle_type  and orientation=column

Drop table if exists ao_ss_ct_t3;
Create table ao_ss_ct_t3 ( i int, j int) with(compresstype=rle_type, orientation=column);


Insert into ao_ss_ct_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t3;

\d+ ao_ss_ct_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t3');

Drop table ao_ss_ct_t3;

-- Create table with a new compresstype at column level for one column 

Drop table if exists ao_ss_ct_t5;
Create table ao_ss_ct_t5 ( i int, j int encoding(compresstype=quicklz));

Insert into ao_ss_ct_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t5;

\d+ ao_ss_ct_t5;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ct_t5'::regclass;

Drop table ao_ss_ct_t5;

-- Create table with compresstype in default column encoding

Drop table if exists ao_ss_ct_t6;
Create table ao_ss_ct_t6 ( i int, j int, DEFAULT COLUMN ENCODING (compresstype=quicklz)) with(orientation=column);

Insert into ao_ss_ct_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t6;

\d+ ao_ss_ct_t6;

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ct_t6'::regclass;

Drop table ao_ss_ct_t6;
-- Create table with invalid value for compresstype

Drop table if exists ao_ss_ct_t4;
Create table ao_ss_ct_t4 ( i int, j int) with(compresstype=xxxx);



-- Create table with orientation,checksum,compresslevel

Drop table if exists ao_ss_ct_t5;
Create table ao_ss_ct_t5 ( i int, j int) with(orientation=column, compresslevel=1, checksum=true);

Insert into ao_ss_ct_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t5;

\d+ ao_ss_ct_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t5');

Drop table ao_ss_ct_t5;


-- ========================================
-- Set the session level guc to quicklz

SET gp_default_storage_options="appendonly=true,compresstype=quicklz";

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_ss_ct_t1;
Create table ao_ss_ct_t1 ( i int, j int);

Insert into ao_ss_ct_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_ss_ct_t1;

\d+ ao_ss_ct_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t1');

Drop table ao_ss_ct_t1;

-- Guc with compresstype quicklz, Alter type with zlib, create table  with new type

Drop table if exists ao_ss_ct_t11;

ALTER TYPE bpchar SET DEFAULT ENCODING (compresstype=zlib);
Create table ao_ss_ct_t11 ( i int, j char(20)) with(orientation=column);

Insert into ao_ss_ct_t11 values (generate_series(1,10) , 'guc with quicklz');

Select count(*) from ao_ss_ct_t11;

\d+ ao_ss_ct_t11;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t11';

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ct_t11'::regclass order by attnum;

ALTER TYPE bpchar SET DEFAULT ENCODING (compresstype=none);

Drop table ao_ss_ct_t11;

-- Guc with compresstype quicklz, Alter type with zlib, Alter column with new type
Drop table if exists ao_ss_ct_t12;
Create table ao_ss_ct_t12 ( i int, j int )  with(orientation=column);

Insert into ao_ss_ct_t12 select i, i+1 from generate_series(1,10) i;

Alter type numeric SET DEFAULT ENCODING (compresstype=zlib);

Alter table ao_ss_ct_t12 add column k numeric default 23;

\d+ ao_ss_ct_t12;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t12';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t1i2');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ct_t12'::regclass order by attnum;

ALTER TYPE numeric SET DEFAULT ENCODING (compresstype=none);

Drop table ao_ss_ct_t12;
-- Set the session level guc to rle_type


SET  gp_default_storage_options="appendonly=true, compresstype=rle_type, orientation=column";

show gp_default_storage_options;

-- Create table with no options 

Drop table if exists ao_ss_ct_t2;
Create table ao_ss_ct_t2 ( i int, j int);

Insert into ao_ss_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t2;

\d+ ao_ss_ct_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t2');

Drop table ao_ss_ct_t2;


-- Create table with appendonly=false

Drop table if exists ao_ss_ct_t3;
Create table ao_ss_ct_t3 ( i int, j int) with(appendonly=false);

Insert into ao_ss_ct_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t3;

\d+ ao_ss_ct_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ct_t3';

Drop table ao_ss_ct_t3;


-- Create table with appendonly=true

Drop table if exists ao_ss_ct_t4;
Create table ao_ss_ct_t4 ( i int, j int) with(appendonly=true);



--- =====================================
-- GUC with invalid value
SET gp_default_storage_options="appendonly=true,compresstype=what";

SET gp_default_storage_options="appendonly=true, compresstype=quicklz, compresslevel=4";

-- GUC with compresstype without appendonly=true
SET gp_default_storage_options="compresstype=quicklz";

show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_ss_ct_t1;
Create table ao_ss_ct_t1( i int, j int);

Insert into ao_ss_ct_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t1;

\d+ ao_ss_ct_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_ct_t1';

Drop table ao_ss_ct_t1;
-- with appendonly=true specified the table will be created as AO table with compresstype

Drop table if exists ao_ss_ct_t2;
Create table ao_ss_ct_t2( i int, j int) with(appendonly=true);
Insert into ao_ss_ct_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ct_t2;

\d+ ao_ss_ct_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_ct_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ct_t2');

Drop table ao_ss_ct_t2;

-- Guc with compresstype zlib, compresslevel=2, Alter type with compresstype rle_type/quicklz

SET gp_default_storage_options="appendonly=true,compresstype=zlib, compresslevel=2";
Alter type numeric SET DEFAULT ENCODING (compresstype=rle_type);

select * from pg_type_encoding where typid= (select oid from pg_type where typname='numeric');

Alter type numeric SET DEFAULT ENCODING (compresstype=quicklz);

select * from pg_type_encoding where typid= (select oid from pg_type where typname='numeric');

Alter type numeric SET DEFAULT ENCODING (compresstype=none);

-- Reset guc to default value 
RESET gp_default_storage_options;
show gp_default_storage_options;
