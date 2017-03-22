-- 
-- @description Guc setting at session level for checksum


-- Guc value checksum=true  (with appendonly=true)

\c dsp_db1

show gp_default_storage_options;

SET gp_default_storage_options="appendonly=true,checksum=true";
show gp_default_storage_options;

-- Table with no options
Drop table if exists  ao_ss_ck_t1;
Create table  ao_ss_ck_t1 ( i int, j int);

Insert into  ao_ss_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t1;

\d+  ao_ss_ck_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ck_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ck_t1');

Drop table  ao_ss_ck_t1;

-- Create table with checksum=true

Drop table if exists  ao_ss_ck_t2;
Create table  ao_ss_ck_t2 ( i int, j int) with(checksum=true);

Insert into  ao_ss_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t2;

\d+  ao_ss_ck_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ck_t2');

Drop table  ao_ss_ck_t2;


-- Create table with checksum=false

Drop table if exists  ao_ss_ck_t3;
Create table  ao_ss_ck_t3 ( i int, j int) with(checksum=false);

Insert into  ao_ss_ck_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t3;

\d+  ao_ss_ck_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ck_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ck_t3');

Drop table  ao_ss_ck_t3;

-- Create table with invalid value for checksum

Drop table if exists  ao_ss_ck_t4;
Create table  ao_ss_ck_t4 ( i int, j int) with(checksum=xxxx);


-- Create a table with appendonly=false

Drop table if exists  ao_ss_ck_t5;
Create table  ao_ss_ck_t5 ( i int, j int) with(appendonly=false);

Insert into  ao_ss_ck_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t5;

\d+  ao_ss_ck_t5;

select relstorage,reloptions from pg_class where relname='ao_ss_ck_t5';

Drop table  ao_ss_ck_t5;

-- Create CO table with checksum true, Alter to add a column with encoding
Drop table if exists  ao_ss_ck_t6;
Create table  ao_ss_ck_t6 ( i int, j int) with(orientation=column);

Insert into  ao_ss_ck_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t6;

\d+  ao_ss_ck_t6;
select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ck_t6';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ck_t6');

Alter table ao_ss_ck_t6 add column k int default 2 encoding(compresstype=quicklz,blocksize=8192);

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ck_t6'::regclass order by attnum;

Drop table ao_ss_ck_t6;

Drop table if exists ao_ss_ck_t7;
Create table ao_ss_ck_t7 (
   a int encoding (compresstype=zlib,compresslevel=2), b float,c varchar encoding(blocksize=8192),d bigint
) with (appendonly=true, orientation=column, checksum=false) distributed by (a);

Insert into ao_ss_ck_t7 values(1, 2.0, 'varchar', 21);

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ck_t7'::regclass order by attnum;

Drop table ao_ss_ck_t7;
-- ========================================
-- Set the session level guc to false


SET gp_default_storage_options="appendonly=true,checksum=false";

show gp_default_storage_options;


-- Table with no options
Drop table if exists  ao_ss_ck_t1;
Create table  ao_ss_ck_t1 ( i int, j int);

Insert into  ao_ss_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t1;

\d+  ao_ss_ck_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ck_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ck_t1');

Drop table  ao_ss_ck_t1;

-- Create table with checksum=true

Drop table if exists  ao_ss_ck_t2;
Create table  ao_ss_ck_t2 ( i int, j int) with(checksum=true);

Insert into  ao_ss_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t2;

\d+  ao_ss_ck_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ck_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ck_t2');

Drop table  ao_ss_ck_t2;


-- Create table with checksum=false

Drop table if exists  ao_ss_ck_t3;
Create table  ao_ss_ck_t3 ( i int, j int) with(checksum=false);

Insert into  ao_ss_ck_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t3;

\d+  ao_ss_ck_t3;

select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='ao_ss_ck_t3';

Drop table  ao_ss_ck_t3;


-- Create table with invalid value for checksum

Drop table if exists  ao_ss_ck_t4;
Create table  ao_ss_ck_t4 ( i int, j int) with(checksum=xxxx);


-- Create table with appendonly=false
Drop table if exists ao_ss_ck_t5;
Create table ao_ss_ck_t5( i int, j int) with(appendonly=false);

select relstorage, reloptions from pg_class where relname='ao_ss_ck_t5';

Drop table  ao_ss_ck_t5;

-- Create CO table with checksum false, Alter to add a column with encoding
Drop table if exists  ao_ss_ck_t6;
Create table  ao_ss_ck_t6 ( i int, j int) with(orientation=column);

Insert into  ao_ss_ck_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from  ao_ss_ck_t6;

\d+  ao_ss_ck_t6;
select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='ao_ss_ck_t6';

Alter table ao_ss_ck_t6 add column k int default 2 encoding(compresstype=quicklz,blocksize=8192);

select attnum, attoptions from pg_attribute_encoding where attrelid='ao_ss_ck_t6'::regclass order by attnum;

Drop table ao_ss_ck_t6;


--- =====================================
-- GUC with invalid value
SET gp_default_storage_options="appendonly=true,checksum=xxx";

-- GUC without appedonly=true specified
SET gp_default_storage_options="checksum=true";

show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_ss_ck_t1;
Create table ao_ss_ck_t1( i int, j int);

Insert into ao_ss_ck_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ck_t1;

\d+ ao_ss_ck_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_ck_t1';

Drop table ao_ss_ck_t1;
-- with appendonly=true specified the table will be created as AO table with checksum=true

Drop table if exists ao_ss_ck_t2;
Create table ao_ss_ck_t2( i int, j int) with(appendonly=true);
Insert into ao_ss_ck_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ck_t2;

\d+ ao_ss_ck_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_ck_t2';

Drop table ao_ss_ck_t2;

-- GUC with appendonly=false
SET gp_default_storage_options="appendonly=false";

show gp_default_storage_options;


Drop table if exists  ao_ss_ck_t5;
Create table  ao_ss_ck_t5 ( i int, j int) with(checksum=true);


-- Reset guc to default value

RESET gp_default_storage_options;
show gp_default_storage_options;
