-- 
-- @description Guc setting at database level for appendonly

-- Guc value appendonly=true
Alter database dsp_db1 set gp_default_storage_options="appendonly=true";

Select datconfig from pg_database where datname='dsp_db1';

\c dsp_db1

show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_db_ap_t1;
Create table ao_db_ap_t1 ( i int, j int);

Create index ap_t1_ix on ao_db_ap_t1(i);

Insert into ao_db_ap_t1 select i, i+1 from generate_series(1,10) i;

update ao_db_ap_t1 set j=i where i>5;

Delete from ao_db_ap_t1 where i<2;

Select count(*) from ao_db_ap_t1;

\d+ ao_db_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ap_t1');


Drop table ao_db_ap_t1;



-- Create table with appendonly=true

Drop table if exists ao_db_ap_t2;
Create table ao_db_ap_t2 ( i int, j int) with(appendonly=true);

Insert into ao_db_ap_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ap_t2;

\d+ ao_db_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ap_t2');

Drop table ao_db_ap_t2;



-- Create table with appendonly=false

Drop table if exists ao_db_ap_t3;
Create table ao_db_ap_t3 ( i int, j int) with(appendonly=false);

Insert into ao_db_ap_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ap_t3;

\d+ ao_db_ap_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t3';

Drop table ao_db_ap_t3;



-- Create table with invalid value for appendonly

Drop table if exists ao_db_ap_t4;
Create table ao_db_ap_t4 ( i int, j int) with(appendonly=xxxx);



-- Create table with orientation,checksum,compresstype,compresslevel, fillfactor

Drop table if exists ao_db_ap_t5;
Create table ao_db_ap_t5 ( i int, j int) with(orientation=column, compresstype=zlib, compresslevel=1, checksum=true, fillfactor=10, oids=false);

Insert into ao_db_ap_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ap_t5;

\d+ ao_db_ap_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ap_t5');


Drop table ao_db_ap_t5;

-- Create temporary table

Create temp table temp_ds_t1 ( i int, j int);

\d+ temp_ds_t1
select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='temp_ds_t1';

Drop table temp_ds_t1;

-- ========================================
-- Set the database level guc to false


Alter database dsp_db2 set gp_default_storage_options="appendonly=false";

Select datconfig from pg_database where datname='dsp_db2';

\c dsp_db2

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_db_ap_t1;
Create table ao_db_ap_t1 ( i int, j int);

Insert into ao_db_ap_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_db_ap_t1;

\d+ ao_db_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t1';

Alter table ao_db_ap_t1 add column k int default 2;

\d+ ao_db_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t1';


SET gp_default_storage_options='appendonly=true, orientation=column, blocksize=8192,compresslevel=0';

Alter table ao_db_ap_t1 add column l int default 3;

\d+ ao_db_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t1';

SET gp_default_storage_options='appendonly=true,compresstype=quicklz';

Alter table ao_db_ap_t1 add column m int default 5;

\d+ ao_db_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t1';

RESET gp_default_storage_options;
show gp_default_storage_options;

Drop table ao_db_ap_t1;



-- Create table with appendonly=true

Drop table if exists ao_db_ap_t2;
Create table ao_db_ap_t2 ( i int, j int) with(appendonly=true);

Insert into ao_db_ap_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ap_t2;

\d+ ao_db_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ap_t2');


Drop table ao_db_ap_t2;



-- Create table with appendonly=false

Drop table if exists ao_db_ap_t3;
Create table ao_db_ap_t3 ( i int, j int) with(appendonly=false);

Insert into ao_db_ap_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ap_t3;

\d+ ao_db_ap_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t3';

Drop table ao_db_ap_t3;



-- Create table with invalid value for appendonly

Drop table if exists ao_db_ap_t4;
Create table ao_db_ap_t4 ( i int, j int) with(appendonly=xxxx);


-- Create table with orientation=column

Drop table if exists ao_db_ap_t5;
Create table ao_db_ap_t5 ( i int, j int) with(orientation=column);

-- Create table with appendonly=true, orientation=column, Alter table add column with encoding

Drop table if exists ao_db_ap_t6;
Create table ao_db_ap_t6 ( i int, j int) with(appendonly=true,orientation=column);

Insert into ao_db_ap_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_db_ap_t6;

\d+ ao_db_ap_t6;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t6';

Alter table ao_db_ap_t6 add column k int default 2 encoding(compresstype=quicklz);

SET gp_default_storage_options='appendonly=false, orientation=row';

Alter table ao_db_ap_t6 add column l int default 5 encoding(compresstype=quicklz);
\d+ ao_db_ap_t6;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_t6';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_db_ap_t6');

Drop table ao_db_ap_t6;

--- =====================================
-- GUC with invalid value
Alter database dsp_db3 set gp_default_storage_options="appendonly=xxx";


-- Reset guc to default value for all three databases

Alter database dsp_db1 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db2 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db3 set gp_default_storage_options TO DEFAULT;
Alter database dsp_db4 set gp_default_storage_options TO DEFAULT;

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;

