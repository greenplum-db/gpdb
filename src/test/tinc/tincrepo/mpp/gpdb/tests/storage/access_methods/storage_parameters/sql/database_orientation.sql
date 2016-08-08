-- 
-- @description Guc setting at database level for orientation

-- Guc value orientation=column (with appendonly=true)

Alter database dsp_db1 set gp_default_storage_options="appendonly=true, orientation=column";

Select datconfig from pg_database where datname='dsp_db1';

\c dsp_db1

show gp_default_storage_options;

-- Create table with no options
Drop table if exists ao_db_or_t1;
Create table ao_db_or_t1 ( i int, j int);

Create index or_t1_ix on ao_db_or_t1(i);

Insert into ao_db_or_t1 select i, i+1 from generate_series(1,10) i;

update ao_db_or_t1 set j=i where i>5;

Delete from ao_db_or_t1 where i<2;

Select count(*) from ao_db_or_t1;

\d+ ao_db_or_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_or_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t1');

Drop table ao_db_or_t1;



-- Create table with orientation=column

Drop table if exists ao_db_or_t2;
Create table ao_db_or_t2 ( i int, j int) with(orientation=column);

Insert into ao_db_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t2;

\d+ ao_db_or_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t2');

Drop table ao_db_or_t2;



-- Create table with orientation=row

Drop table if exists ao_db_or_t3;
Create table ao_db_or_t3 ( i int, j int) with(orientation=row);

Insert into ao_db_or_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t3;

\d+ ao_db_or_t3;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t3');

Drop table ao_db_or_t3;



-- Create table with invalid value for orientation

Drop table if exists ao_db_or_t4;
Create table ao_db_or_t4 ( i int, j int) with(orientation=xxxx);



-- Create table with tablelevel appendonly=false

Drop table if exists ao_db_or_t5;
Create table ao_db_or_t5( i int, j int) with(appendonly=false);

\d+ ao_db_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t5';
Drop table ao_db_or_t5;


-- ========================================
-- Set the database level guc orientation=row (with appendonly=true)


Alter database dsp_db2 set gp_default_storage_options="appendonly=true, orientation=row";

Select datconfig from pg_database where datname='dsp_db2';

\c dsp_db2

show gp_default_storage_options;

-- Create table with no options
Drop table if exists ao_db_or_t1;
Create table ao_db_or_t1 ( i int, j int);

Insert into ao_db_or_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t1;

\d+ ao_db_or_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t1');

Alter table ao_db_or_t1 add column k int default 10;

\d+ ao_db_or_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t1');

Drop table ao_db_or_t1;



-- Create table with orientation=row

Drop table if exists ao_db_or_t2;
Create table ao_db_or_t2 ( i int, j int) with(orientation=row);

Insert into ao_db_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t2;

\d+ ao_db_or_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t2');

Drop table ao_db_or_t2;



-- Create table with orientation=column

Drop table if exists ao_db_or_t3;
Create table ao_db_or_t3 ( i int, j int) with(orientation=column);

Insert into ao_db_or_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t3;

\d+ ao_db_or_t3;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t3');

Drop table ao_db_or_t3;



-- Create table with invalid value for orientation

Drop table if exists ao_db_or_t4;
Create table ao_db_or_t4 ( i int, j int) with(orientation=xxxx);



-- Create table with tablelevel appendonly=false

Drop table if exists ao_db_or_t5;
Create table ao_db_or_t5( i int, j int) with(appendonly=false);

\d+ ao_db_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t5';

Alter table ao_db_or_t5 add column k int default 10;

\d+ ao_db_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t5';

Drop table ao_db_or_t5;

-- Create table with compresstype=rle_type
Drop table if exists ao_db_or_t6;
Create table ao_db_or_t6( i int, j int) with(compresstype=rle_type);



-- GUC with invalid value
Alter database dsp_db3 set gp_default_storage_options="appendonly=true,orientation=xxx";


-- GUC with appenonly=false
Alter database dsp_db3 set gp_default_storage_options="appendonly=false";

Select datconfig from pg_database where datname='dsp_db3';

\c dsp_db3

show gp_default_storage_options;

Drop table if exists ao_db_or_t6;
Create table ao_db_or_t6( i int, j int) with(orientation=column);

Drop table if exists ao_db_or_t7;
Create table ao_db_or_t7( i int, j int) with(orientation=row);


-- Guc with no appendonly=true specified
Alter database dsp_db4 set gp_default_storage_options="orientation=column";

\c dsp_db4
show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_db_or_t1;
Create table ao_db_or_t1( i int, j int);

Insert into ao_db_or_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t1;

\d+ ao_db_or_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t1';

Drop table ao_db_or_t1;
-- with appendonly=true specified the table will be created as CO table

Drop table if exists ao_db_or_t2;
Create table ao_db_or_t2( i int, j int) with(appendonly=true);
Insert into ao_db_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_or_t2;

\d+ ao_db_or_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_or_t2');

Drop table ao_db_or_t2;

--- =====================================

-- Reset guc to default value for all four  databases
Alter database dsp_db1 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db2 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db3 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db4 set gp_default_storage_options TO DEFAULT;

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
