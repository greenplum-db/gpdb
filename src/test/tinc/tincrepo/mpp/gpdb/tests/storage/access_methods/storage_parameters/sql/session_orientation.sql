-- 
-- @description Guc setting at session level for orientation

-- Guc value orientation=column (with appendonly=true)

 
\c dsp_db1

show gp_default_storage_options;

SET gp_default_storage_options="appendonly=true, orientation=column";

-- Create table with no options
Drop table if exists ao_ss_or_t1;
Create table ao_ss_or_t1 ( i int, j int);

Create index or_t1_ix on ao_ss_or_t1(i);

Insert into ao_ss_or_t1 select i, i+1 from generate_series(1,10) i;

update ao_ss_or_t1 set j=i where i>5;

Delete from ao_ss_or_t1 where i<2;

Select count(*) from ao_ss_or_t1;

\d+ ao_ss_or_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_or_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_or_t1');

Drop table ao_ss_or_t1;



-- Create table with orientation=column

Drop table if exists ao_ss_or_t2;
Create table ao_ss_or_t2 ( i int, j int) with(orientation=column);

Insert into ao_ss_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_or_t2;

\d+ ao_ss_or_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_or_t2');

Drop table ao_ss_or_t2;



-- Create table with orientation=row

Drop table if exists ao_ss_or_t3;
Create table ao_ss_or_t3 ( i int, j int) with(orientation=row);

Insert into ao_ss_or_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_or_t3;

\d+ ao_ss_or_t3;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_or_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_or_t3');

Drop table ao_ss_or_t3;



-- Create table with invalid value for orientation

Drop table if exists ao_ss_or_t4;
Create table ao_ss_or_t4 ( i int, j int) with(orientation=xxxx);



-- Create table with tablelevel appendonly=false

Drop table if exists ao_ss_or_t5;
Create table ao_ss_or_t5( i int, j int) with(appendonly=false);

\d+ ao_ss_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_ss_or_t5';
Drop table ao_ss_or_t5;


-- ========================================
-- Set the session level guc orientation=row (with appendonly=true)


SET gp_default_storage_options="appendonly=true, orientation=row";

show gp_default_storage_options;

-- Create table with no options
Drop table if exists ao_ss_or_t1;
Create table ao_ss_or_t1 ( i int, j int);

Insert into ao_ss_or_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_or_t1;

\d+ ao_ss_or_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_or_t1';

Drop table ao_ss_or_t1;



-- Create table with orientation=row

Drop table if exists ao_ss_or_t2;
Create table ao_ss_or_t2 ( i int, j int) with(orientation=row);

Insert into ao_ss_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_or_t2;

\d+ ao_ss_or_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_or_t2');

Drop table ao_ss_or_t2;



-- Create table with orientation=column

Drop table if exists ao_ss_or_t3;
Create table ao_ss_or_t3 ( i int, j int) with(orientation=column);

Insert into ao_ss_or_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_or_t3;

\d+ ao_ss_or_t3;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_or_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_or_t3');

Drop table ao_ss_or_t3;



-- Create table with invalid value for orientation

Drop table if exists ao_ss_or_t4;
Create table ao_ss_or_t4 ( i int, j int) with(orientation=xxxx);



-- Create table with tablelevel appendonly=false

Drop table if exists ao_ss_or_t5;
Create table ao_ss_or_t5( i int, j int) with(appendonly=false);

\d+ ao_ss_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_ss_or_t5';
Drop table ao_ss_or_t5;


-- Create table with compresstype=rle_type
Drop table if exists ao_ss_or_t6;
Create table ao_ss_or_t6( i int, j int) with(compresstype=rle_type);



-- Set GUC with invalid value
SET gp_default_storage_options="appendonly=true,orientation=xxx";


-- GUC with appenonly=false
SET gp_default_storage_options="appendonly=false";

show gp_default_storage_options;

Drop table if exists ao_ss_or_t6;
Create table ao_ss_or_t6( i int, j int) with(orientation=column);

Drop table if exists ao_ss_or_t7;
Create table ao_ss_or_t7( i int, j int) with(orientation=row);

-- GUC without appendonly=true
SET gp_default_storage_options="orientation=column";

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
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_or_t2');

Drop table ao_db_or_t2;


--- =====================================

-- Reset guc to default value 

RESET gp_default_storage_options;
show gp_default_storage_options;
