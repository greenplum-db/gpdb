-- 
-- @description Guc setting at role level for checksum

-- Set the GUCs ahead as super user to avoid permission issues

-- Alter the roles to set their gp_default_configuration options
select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;

-- Set the role level guc to true
Alter role dsp_role1 set gp_default_storage_options="appendonly=true,checksum=true";

-- Set the role level guc to false

Alter role dsp_role2 set gp_default_storage_options="appendonly=true,checksum=false";

-- GUC with appendonly=false
Alter role dsp_role3 set gp_default_storage_options="appendonly=false";

-- GUC with invalid value

Alter role dsp_role4 set  gp_default_storage_options="appendonly=true,checksum=xxx";

-- Guc without appenodnly=true
Alter role dsp_role4 set gp_default_storage_options="checksum=true";

-- ==================================


select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;

-- Role with Guc value checksum=true  (with appendonly=true)

\c dsp_db1 dsp_role1

show gp_default_storage_options;

-- Table with no options
Drop table if exists  ao_rl_ck_t1;
Create table  ao_rl_ck_t1 ( i int, j int);

Insert into  ao_rl_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t1;

\d+  ao_rl_ck_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ck_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ck_t1');

Drop table  ao_rl_ck_t1;

-- Create table with checksum=true

Drop table if exists  ao_rl_ck_t2;
Create table  ao_rl_ck_t2 ( i int, j int) with(checksum=true);

Insert into  ao_rl_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t2;

\d+  ao_rl_ck_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ck_t2');

Drop table  ao_rl_ck_t2;


-- Create table with checksum=false

Drop table if exists  ao_rl_ck_t3;
Create table  ao_rl_ck_t3 ( i int, j int) with(checksum=false);

Insert into  ao_rl_ck_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t3;

\d+  ao_rl_ck_t3;

select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='ao_rl_ck_t3';

Drop table  ao_rl_ck_t3;

-- Create table with invalid value for checksum

Drop table if exists  ao_rl_ck_t4;
Create table  ao_rl_ck_t4 ( i int, j int) with(checksum=xxxx);


-- Create a table with appendonly=false

Drop table if exists  ao_rl_ck_t5;
Create table  ao_rl_ck_t5 ( i int, j int) with(appendonly=false);

Insert into  ao_rl_ck_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t5;

\d+  ao_rl_ck_t5;

select relstorage,reloptions from pg_class where relname='ao_rl_ck_t5';

Drop table  ao_rl_ck_t5;



-- ========================================
-- Role with role level guc to false


\c dsp_db2 dsp_role2
show gp_default_storage_options;


-- Table with no options
Drop table if exists  ao_rl_ck_t1;
Create table  ao_rl_ck_t1 ( i int, j int);

Insert into  ao_rl_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t1;

\d+  ao_rl_ck_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ck_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ck_t1');

Drop table  ao_rl_ck_t1;

-- Create table with checksum=true

Drop table if exists  ao_rl_ck_t2;
Create table  ao_rl_ck_t2 ( i int, j int) with(checksum=true);

Insert into  ao_rl_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t2;

\d+  ao_rl_ck_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ck_t2');

Drop table  ao_rl_ck_t2;


-- Create table with checksum=false

Drop table if exists  ao_rl_ck_t3;
Create table  ao_rl_ck_t3 ( i int, j int) with(checksum=false);

Insert into  ao_rl_ck_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_rl_ck_t3;

\d+  ao_rl_ck_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_rl_ck_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ck_t3');

Drop table  ao_rl_ck_t3;


-- Create table with invalid value for checksum

Drop table if exists  ao_rl_ck_t4;
Create table  ao_rl_ck_t4 ( i int, j int) with(checksum=xxxx);


-- Create table with appendonly=false
Drop table if exists ao_rl_ck_t5;
Create table ao_rl_ck_t5( i int, j int) with(appendonly=false);

select relstorage, reloptions from pg_class where relname='ao_rl_ck_t5';

Drop table  ao_rl_ck_t5;

--- =====================================


-- Role with GUC  appendonly=false

\c dsp_db3 dsp_role3
show gp_default_storage_options;


Drop table if exists  ao_rl_ck_t5;
Create table  ao_rl_ck_t5 ( i int, j int) with(checksum=true);

-- Guc with no appendonly=true specified

\c dsp_db4 dsp_role4
show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_rl_ck_t1;
Create table ao_rl_ck_t1( i int, j int);

Insert into ao_rl_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ck_t1;

\d+ ao_rl_ck_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ck_t1';

Drop table ao_rl_ck_t1;
-- with appendonly=true specified the table will be created as AO table with checksum=true

Drop table if exists ao_rl_ck_t2;
Create table ao_rl_ck_t2( i int, j int) with(appendonly=true);
Insert into ao_rl_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_rl_ck_t2;

\d+ ao_rl_ck_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_rl_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_rl_ck_t2');

Drop table ao_rl_ck_t2;


