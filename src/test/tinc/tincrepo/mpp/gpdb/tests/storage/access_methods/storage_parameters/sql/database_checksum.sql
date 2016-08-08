-- 
-- @description Guc setting at database level for checksum

-- Guc value checksum=true  (with appendonly=true)
Alter database dsp_db1 set gp_default_storage_options="appendonly=true,checksum=true";

Select datconfig from pg_database where datname='dsp_db1';

\c dsp_db1

show gp_default_storage_options;

-- Table with no options
Drop table if exists  ao_db_ck_t1;
Create table  ao_db_ck_t1 ( i int, j int);

Insert into  ao_db_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t1;

\d+  ao_db_ck_t1;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t1');

Drop table  ao_db_ck_t1;

-- Create table with checksum=true

Drop table if exists  ao_db_ck_t2;
Create table  ao_db_ck_t2 ( i int, j int) with(checksum=true);

Insert into  ao_db_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t2;

\d+  ao_db_ck_t2;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t2');
-- Alter add a column

Alter table ao_db_ck_t2 add column k int default 6;

\d+  ao_db_ck_t2;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t2');

Drop table  ao_db_ck_t2;


-- Create table with checksum=false

Drop table if exists  ao_db_ck_t3;
Create table  ao_db_ck_t3 ( i int, j int) with(checksum=false);

Insert into  ao_db_ck_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t3;

\d+  ao_db_ck_t3;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t3');

Drop table  ao_db_ck_t3;

-- Create table with invalid value for checksum

Drop table if exists  ao_db_ck_t4;
Create table  ao_db_ck_t4 ( i int, j int) with(checksum=xxxx);


-- Create a table with appendonly=false

Drop table if exists  ao_db_ck_t5;
Create table  ao_db_ck_t5 ( i int, j int) with(appendonly=false);

Insert into  ao_db_ck_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t5;

\d+  ao_db_ck_t5;

select relstorage,reloptions from pg_class where relname='ao_db_ck_t5';

Drop table  ao_db_ck_t5;



-- ========================================
-- Set the database level guc to false


Alter database dsp_db2 set gp_default_storage_options="appendonly=true,checksum=false";

Select datconfig from pg_database where datname='dsp_db2';

\c dsp_db2
show gp_default_storage_options;

-- Table with no options
Drop table if exists  ao_db_ck_t1;
Create table  ao_db_ck_t1 ( i int, j int);

Insert into  ao_db_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t1;

\d+  ao_db_ck_t1;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t1');

Drop table  ao_db_ck_t1;

-- Create table with checksum=true

Drop table if exists  ao_db_ck_t2;
Create table  ao_db_ck_t2 ( i int, j int) with(checksum=true);

Insert into  ao_db_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t2;

\d+  ao_db_ck_t2;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t2');

Drop table  ao_db_ck_t2;


-- Create table with checksum=false

Drop table if exists  ao_db_ck_t3;
Create table  ao_db_ck_t3 ( i int, j int) with(checksum=false);

Insert into  ao_db_ck_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_db_ck_t3;

\d+  ao_db_ck_t3;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t3');

Drop table  ao_db_ck_t3;


-- Create table with invalid value for checksum

Drop table if exists  ao_db_ck_t4;
Create table  ao_db_ck_t4 ( i int, j int) with(checksum=xxxx);


-- Create table with appendonly=false
Drop table if exists ao_db_ck_t5;
Create table ao_db_ck_t5( i int, j int) with(appendonly=false);

select relstorage, reloptions from pg_class where relname='ao_db_ck_t5';

Drop table  ao_db_ck_t5;

--- =====================================
-- GUC with invalid value
Alter database dsp_db3 set gp_default_storage_options="appendonly=true,checksum=xxx";

-- GUC with appendonly=false
Alter database dsp_db3 set gp_default_storage_options="appendonly=false";

\c dsp_db3

show gp_default_storage_options;


Drop table if exists  ao_db_ck_t5;
Create table  ao_db_ck_t5 ( i int, j int) with(checksum=true);

-- Guc with no appendonly=true specified
Alter database dsp_db4 set gp_default_storage_options="checksum=true";

\c dsp_db4
show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_db_ck_t1;
Create table ao_db_ck_t1( i int, j int);

Insert into ao_db_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ck_t1;

\d+ ao_db_ck_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_db_ck_t1';

Drop table ao_db_ck_t1;
-- with appendonly=true specified the table will be created as AO table with checksum=true

Drop table if exists ao_db_ck_t2;
Create table ao_db_ck_t2( i int, j int) with(appendonly=true);
Insert into ao_db_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ck_t2;

\d+ ao_db_ck_t2;

select relstorage, reloptions from pg_class where relname='ao_db_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ck_t2');

Drop table ao_db_ck_t2;


-- Reset guc to default value for all three databases

Alter database dsp_db1 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db2 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db3 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db4 set gp_default_storage_options TO DEFAULT;

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
