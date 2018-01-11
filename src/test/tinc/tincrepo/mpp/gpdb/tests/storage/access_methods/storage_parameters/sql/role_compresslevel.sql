-- 
-- @description Guc setting at role level for compresslevel


-- Guc value to valid value compresslevel=1
Alter role dsp_role1 set gp_default_storage_options="appendonly=true, compresslevel=1";

-- Set role level Guc to 2, and create compresstype=quicklz

Alter role dsp_role3 set gp_default_storage_options="appendonly=true, compresslevel=2";

-- GUC with appendonly=true not specified
Alter role dsp_role4 set gp_default_storage_options="compresslevel=1";


select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;

\c dsp_db1 dsp_role1

show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_rl_cl_t1;
Create table ao_rl_cl_t1 ( i int, j int);

Create index ap_t1_ix on ao_rl_cl_t1(i);

Insert into ao_rl_cl_t1 select i, i+1 from generate_series(1,10) i;

update ao_rl_cl_t1 set j=i where i>5;

Delete from ao_rl_cl_t1 where i<2;

Select count(*) from ao_rl_cl_t1;

\d+ ao_rl_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t1');


Alter table ao_rl_cl_t1 add column k int default 2;

\d+ ao_rl_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t1');


SET gp_default_storage_options='appendonly=true, orientation=column, blocksize=8192,compresslevel=0';

Alter table ao_rl_cl_t1 add column l int default 3;

\d+ ao_rl_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t1');

SET gp_default_storage_options='appendonly=false,compresstype=quicklz';

Alter table ao_rl_cl_t1 add column m int default 5;

\d+ ao_rl_cl_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t1');


RESET gp_default_storage_options;
show gp_default_storage_options;

Drop table ao_rl_cl_t1;



-- Create table with compresstype=quicklz

Drop table if exists ao_rl_cl_t2;
Create table ao_rl_cl_t2 ( i int, j int) with(compresstype=quicklz);

Insert into ao_rl_cl_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t2;

\d+ ao_rl_cl_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t2');

Drop table ao_rl_cl_t2;


-- Create table with rle_type, orientation=column


Drop table if exists ao_rl_cl_t3;
Create table ao_rl_cl_t3 ( i int, j int) with(compresstype=rle_type, orientation=column);

Insert into ao_rl_cl_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t3;

\d+ ao_rl_cl_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t3');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_rl_cl_t3'::regclass;

Drop table ao_rl_cl_t3;


-- Create table with compresstype=zlib

Drop table if exists ao_rl_cl_t6;
Create table ao_rl_cl_t6 ( i int, j int) with(compresstype=zlib);

Insert into ao_rl_cl_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t6;

\d+ ao_rl_cl_t6;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t6';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t6');

Drop table ao_rl_cl_t6;


-- Create table with appendonly=false

Drop table if exists ao_rl_cl_t4;
Create table ao_rl_cl_t4 ( i int, j int) with(appendonly=false);

Insert into ao_rl_cl_t4 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t4;

\d+ ao_rl_cl_t4;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t4';

Drop table ao_rl_cl_t4;


-- Create table with orientation and checksum

Drop table if exists ao_rl_cl_t5;
Create table ao_rl_cl_t5 ( i int, j int) with(orientation=column, checksum=true);

Insert into ao_rl_cl_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t5;

\d+ ao_rl_cl_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t5');

Drop table ao_rl_cl_t5;


-- Create table with compresstype in encoding clause, add new column with new compresslevel


Drop table if exists ao_rl_cl_t7;
Create table ao_rl_cl_t7 ( i int, j int encoding(compresstype=quicklz)) with(orientation=column);

Insert into ao_rl_cl_t7 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t7;

\d+ ao_rl_cl_t7;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t7';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t7');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_rl_cl_t7'::regclass;


Alter table ao_rl_cl_t7 add column k int default 3 encoding(compresstype=rle_type, compresslevel=3) ;
Alter table ao_rl_cl_t7 add column l int default 2 encoding(compresslevel=5);

\d+ ao_rl_cl_t7;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_cl_t7';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t7');

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_rl_cl_t7'::regclass;

Drop table ao_rl_cl_t7;


-- ========================
-- Guc with compresslevel = 2

\c dsp_db2 dsp_role3

show gp_default_storage_options;


-- Create table with compresstype=quicklz

Drop table if exists ao_rl_cl_t1;
Create table ao_rl_cl_t1 ( i int, j int) with(compresstype=quicklz);


-- Guc with no appendonly=true specified
\c dsp_db3 dsp_role4
show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_rl_cl_t1;
Create table ao_rl_cl_t1( i int, j int);

Insert into ao_rl_cl_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t1;

\d+ ao_rl_cl_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_cl_t1';

Drop table ao_rl_cl_t1;
-- with appendonly=true specified the table will be created as AO table with compresslevel

Drop table if exists ao_rl_cl_t2;
Create table ao_rl_cl_t2( i int, j int) with(appendonly=true);
Insert into ao_rl_cl_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_cl_t2;

\d+ ao_rl_cl_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_cl_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_cl_t2');

Drop table ao_rl_cl_t2;
