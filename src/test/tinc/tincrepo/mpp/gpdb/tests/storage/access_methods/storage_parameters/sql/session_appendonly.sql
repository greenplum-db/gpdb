-- 
-- @description Guc setting at session level for appendonly


-- Guc value appendonly=true
\c dsp_db1

show gp_default_storage_options;

SET gp_default_storage_options="appendonly=true";
show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_ss_ap_t1;
Create table ao_ss_ap_t1 ( i int, j int);

Create index ap_t1_ix on ao_ss_ap_t1(i);

Insert into ao_ss_ap_t1 select i, i+1 from generate_series(1,10) i;

update ao_ss_ap_t1 set j=i where i>5;

Delete from ao_ss_ap_t1 where i<2;

Select count(*) from ao_ss_ap_t1;

\d+ ao_ss_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ap_t1');

Drop table ao_ss_ap_t1;



-- Create table with appendonly=true

Drop table if exists ao_ss_ap_t2;
Create table ao_ss_ap_t2 ( i int, j int) with(appendonly=true);

Insert into ao_ss_ap_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ap_t2;

\d+ ao_ss_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ap_t2');

Drop table ao_ss_ap_t2;



-- Create table with appendonly=false

Drop table if exists ao_ss_ap_t3;
Create table ao_ss_ap_t3 ( i int, j int) with(appendonly=false);

Insert into ao_ss_ap_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ap_t3;

\d+ ao_ss_ap_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t3';

Drop table ao_ss_ap_t3;



-- Create table with invalid value for appendonly

Drop table if exists ao_ss_ap_t4;
Create table ao_ss_ap_t4 ( i int, j int) with(appendonly=xxxx);



-- Create table with orientation,checksum,compresstype,compresslevel

Drop table if exists ao_ss_ap_t5;
Create table ao_ss_ap_t5 ( i int, j int) with(orientation=column, compresstype=zlib, compresslevel=1, checksum=true);

Insert into ao_ss_ap_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ap_t5;

\d+ ao_ss_ap_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ap_t5');

Drop table ao_ss_ap_t5;


-- ========================================
-- Set the session level guc to appendonly=false

SET gp_default_storage_options="appendonly=false";

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_ss_ap_t1;
Create table ao_ss_ap_t1 ( i int, j int);

Insert into ao_ss_ap_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_ss_ap_t1;

\d+ ao_ss_ap_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t1';

Drop table ao_ss_ap_t1;



-- Create table with appendonly=true, Alter add a column

Drop table if exists ao_ss_ap_t2;
Create table ao_ss_ap_t2 ( i int, j int) with(appendonly=true);

Insert into ao_ss_ap_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ap_t2;

\d+ ao_ss_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_ap_t2');

show gp_default_storage_options;

Alter table ao_ss_ap_t2 add column k int default 10;

\d+ ao_ss_ap_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t2';

Drop table ao_ss_ap_t2;



-- Create table with appendonly=false

Drop table if exists ao_ss_ap_t3;
Create table ao_ss_ap_t3 ( i int, j int) with(appendonly=false);

Insert into ao_ss_ap_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ap_t3;

\d+ ao_ss_ap_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t3';

Drop table ao_ss_ap_t3;



-- Create table with invalid value for appendonly

Drop table if exists ao_ss_ap_t4;
Create table ao_ss_ap_t4 ( i int, j int) with(appendonly=xxxx);


-- Create table with orientation=column

Drop table if exists ao_ss_ap_t5;
Create table ao_ss_ap_t5 ( i int, j int) with(orientation=column);


-- Create table with appendonly=false, set GUC to appendonly=true, Add a column
Drop table if exists ao_ss_ap_t6;
Create table ao_ss_ap_t6 ( i int, j int) with(appendonly=false);

Insert into ao_ss_ap_t6 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_ap_t6;

\d+ ao_ss_ap_t6;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t6';

SET gp_default_storage_options="appendonly=true, blocksize=8192";

show gp_default_storage_options;

Alter table ao_ss_ap_t6 add column k int default 10;

\d+ ao_ss_ap_t6;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_ap_t6';

Drop table ao_ss_ap_t6;

--- =====================================
-- GUC with invalid value

SET gp_default_storage_options="appendonly=xxx";


-- Reset guc to default value 

RESET gp_default_storage_options;
show gp_default_storage_options;
