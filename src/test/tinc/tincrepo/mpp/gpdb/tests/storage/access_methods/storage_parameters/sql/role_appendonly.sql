-- 
-- @description Guc setting at role level for appendonly

-- Set the GUCs ahead as super user to avoid permission issues

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;

-- Set the role level guc to true
Alter role dsp_role1 set gp_default_storage_options="appendonly=true";

-- Set the role level guc to false

Alter role dsp_role2 set gp_default_storage_options="appendonly=false";


-- GUC with invalid value, value without appendonly=true 

Alter role dsp_role3 set  gp_default_storage_options="appendonly=xxx";

-- ==================================

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3') order by datname;
select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3') order by rolname;



-- Role with Guc value appendonly=true


\c dsp_db1 dsp_role1

show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_rl_ap_t1;
Create table ao_rl_ap_t1 ( i int, j int);

Create index rl_t1_ix on ao_rl_ap_t1(i);

Insert into ao_rl_ap_t1 select i, i+1 from generate_series(1,10) i;

update ao_rl_ap_t1 set j=i where i>5;

Delete from ao_rl_ap_t1 where i<2;

Select count(*) from ao_rl_ap_t1;

\d+ ao_rl_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ap_t1');

Drop table ao_rl_ap_t1;



-- Create table with appendonly=true

Drop table if exists ao_rl_ap_t2;
Create table ao_rl_ap_t2 ( i int, j int) with(appendonly=true);

Insert into ao_rl_ap_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ap_t2;

\d+ ao_rl_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ap_t2');

Drop table ao_rl_ap_t2;



-- Create table with appendonly=false

Drop table if exists ao_rl_ap_t3;
Create table ao_rl_ap_t3 ( i int, j int) with(appendonly=false);

Insert into ao_rl_ap_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ap_t3;

\d+ ao_rl_ap_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t3';

Drop table ao_rl_ap_t3;



-- Create table with invalid value for appendonly

Drop table if exists ao_rl_ap_t4;
Create table ao_rl_ap_t4 ( i int, j int) with(appendonly=xxxx);



-- Create table with orientation,checksum,compresstype,compresslevel

Drop table if exists ao_rl_ap_t5;
Create table ao_rl_ap_t5 ( i int, j int) with(orientation=column, compresstype=zlib, compresslevel=1, checksum=true);

Insert into ao_rl_ap_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ap_t5;

\d+ ao_rl_ap_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ap_t5');

Drop table ao_rl_ap_t5;


-- ========================================
-- Role level guc with appendonly=false


\c dsp_db2 dsp_role2

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_rl_ap_t1;
Create table ao_rl_ap_t1 ( i int, j int);

Insert into ao_rl_ap_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_rl_ap_t1;

\d+ ao_rl_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t1';

Drop table ao_rl_ap_t1;



-- Create table with appendonly=true

Drop table if exists ao_rl_ap_t2;
Create table ao_rl_ap_t2 ( i int, j int) with(appendonly=true);

Insert into ao_rl_ap_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ap_t2;

\d+ ao_rl_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ap_t2');

Drop table ao_rl_ap_t2;



-- Create table with appendonly=false

Drop table if exists ao_rl_ap_t3;
Create table ao_rl_ap_t3 ( i int, j int) with(appendonly=false);

Insert into ao_rl_ap_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ap_t3;

\d+ ao_rl_ap_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ap_t3';

Drop table ao_rl_ap_t3;



-- Create table with invalid value for appendonly

Drop table if exists ao_rl_ap_t4;
Create table ao_rl_ap_t4 ( i int, j int) with(appendonly=xxxx);


-- Create table with orientation=column

Drop table if exists ao_rl_ap_t5;
Create table ao_rl_ap_t5 ( i int, j int) with(orientation=column);
