-- 
-- @description Guc setting at role level for orientation

-- Set the GUCs ahead as super user to avoid permission issues

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;

-- Set the role level guc to column
Alter role dsp_role1 set gp_default_storage_options="appendonly=true, orientation=column";

-- Set the role level guc to row

Alter role dsp_role2 set gp_default_storage_options="appendonly=true, orientation=row";

-- GUC with invalid value 

Alter role dsp_role3 set  gp_default_storage_options="appendonly=true,orientation=xxx";

-- GUC without appendonly=true

Alter role dsp_role3 set gp_default_storage_options="orientation=column";

-- GUC with appenonly=false
Alter role dsp_role4 set gp_default_storage_options="appendonly=false";


\c dsp_db1 dsp_role1

show gp_default_storage_options;

-- Create table with no options
Drop table if exists ao_rl_or_t1;
Create table ao_rl_or_t1 ( i int, j int);

Create index or_t1_ix on ao_rl_or_t1(i);

Insert into ao_rl_or_t1 select i, i+1 from generate_series(1,10) i;

update ao_rl_or_t1 set j=i where i>5;

Delete from ao_rl_or_t1 where i<2;

Select count(*) from ao_rl_or_t1;

\d+ ao_rl_or_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_or_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t1');

Drop table ao_rl_or_t1;


-- Create table with orientation=column

Drop table if exists ao_rl_or_t2;
Create table ao_rl_or_t2 ( i int, j int) with(orientation=column);

Insert into ao_rl_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t2;

\d+ ao_rl_or_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t2');

Drop table ao_rl_or_t2;


-- Create table with orientation=row

Drop table if exists ao_rl_or_t3;
Create table ao_rl_or_t3 ( i int, j int) with(orientation=row);

Insert into ao_rl_or_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t3;

\d+ ao_rl_or_t3;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t3');

Drop table ao_rl_or_t3;


-- Create table with invalid value for orientation

Drop table if exists ao_rl_or_t4;
Create table ao_rl_or_t4 ( i int, j int) with(orientation=xxxx);



-- Create table with tablelevel appendonly=false

Drop table if exists ao_rl_or_t5;
Create table ao_rl_or_t5( i int, j int) with(appendonly=false);

\d+ ao_rl_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t5';
Drop table ao_rl_or_t5;


-- ========================================
-- Role level guc orientation=row (with appendonly=true)


\c dsp_db2 dsp_role2

show gp_default_storage_options;

-- Create table with no options
Drop table if exists ao_rl_or_t1;
Create table ao_rl_or_t1 ( i int, j int);

Insert into ao_rl_or_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t1;

\d+ ao_rl_or_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t1');

Drop table ao_rl_or_t1;



-- Create table with orientation=row

Drop table if exists ao_rl_or_t2;
Create table ao_rl_or_t2 ( i int, j int) with(orientation=row);

Insert into ao_rl_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t2;

\d+ ao_rl_or_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t2');

Drop table ao_rl_or_t2;



-- Create table with orientation=column

Drop table if exists ao_rl_or_t3;
Create table ao_rl_or_t3 ( i int, j int) with(orientation=column);

Insert into ao_rl_or_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t3;

\d+ ao_rl_or_t3;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t3');

Drop table ao_rl_or_t3;



-- Create table with invalid value for orientation

Drop table if exists ao_rl_or_t4;
Create table ao_rl_or_t4 ( i int, j int) with(orientation=xxxx);



-- Create table with tablelevel appendonly=false

Drop table if exists ao_rl_or_t5;
Create table ao_rl_or_t5( i int, j int) with(appendonly=false);

\d+ ao_rl_or_t5
select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t5';
Drop table ao_rl_or_t5;


-- Create table with compresstype=rle_type
Drop table if exists ao_rl_or_t6;
Create table ao_rl_or_t6( i int, j int) with(compresstype=rle_type);


-- Role level Guc with appendonly=false

\c dsp_db3 dsp_role4

show gp_default_storage_options;

Drop table if exists ao_rl_or_t6;
Create table ao_rl_or_t6( i int, j int) with(orientation=column);

Drop table if exists ao_rl_or_t7;
Create table ao_rl_or_t7( i int, j int) with(orientation=row);

\c dsp_db4 dsp_role3

show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_rl_or_t1;
Create table ao_rl_or_t1( i int, j int);

Insert into ao_rl_or_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t1;

\d+ ao_rl_or_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t1';

Drop table ao_rl_or_t1;
-- with appendonly=true specified the table will be created as CO table

Drop table if exists ao_rl_or_t2;
Create table ao_rl_or_t2( i int, j int) with(appendonly=true);
Insert into ao_rl_or_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_or_t2;

\d+ ao_rl_or_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_or_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_or_t2');

Drop table ao_rl_or_t2;
