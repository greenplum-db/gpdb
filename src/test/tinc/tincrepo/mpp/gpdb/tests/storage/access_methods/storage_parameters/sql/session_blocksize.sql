-- 
-- @description Guc setting at session level for blocksize (default blocksize is 32k)


-- Guc value blocksize= 64k

\c dsp_db1

show gp_default_storage_options;

SET gp_default_storage_options="appendonly=true, blocksize=65536";
show gp_default_storage_options;

-- Table with no options
Drop table if exists ao_ss_bk_t1;
Create table ao_ss_bk_t1 ( i int, j int);

Create index ap_t1_ix on ao_ss_bk_t1(i);

Insert into ao_ss_bk_t1 select i, i+1 from generate_series(1,10) i;

update ao_ss_bk_t1 set j=i where i>5;

Delete from ao_ss_bk_t1 where i<2;

Select count(*) from ao_ss_bk_t1;

\d+ ao_ss_bk_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_bk_t1');

Drop table ao_ss_bk_t1;



-- Create table with blocksize=8k

Drop table if exists ao_ss_bk_t2;
Create table ao_ss_bk_t2 ( i int, j int) with(blocksize=8192);

Insert into ao_ss_bk_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t2;

\d+ ao_ss_bk_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_bk_t2');

Drop table ao_ss_bk_t2;



-- Create table with blocksize=64k

Drop table if exists ao_ss_bk_t3;
Create table ao_ss_bk_t3 ( i int, j int) with(blocksize=65536);

Insert into ao_ss_bk_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t3;

\d+ ao_ss_bk_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t3';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_bk_t3');

Drop table ao_ss_bk_t3;



-- Create table with invalid value for blocksize (only multiples of 8k are valid values)

Drop table if exists ao_ss_bk_t4;
Create table ao_ss_bk_t4 ( i int, j int) with(blocksize=3456);


-- Create table with other storage parameters , orientation, checksum, compresstype, compresslevel
Drop table if exists ao_ss_bk_t5;
Create table ao_ss_bk_t5 ( i int, j int) with(orientation=column, compresstype=zlib, compresslevel=1, checksum=true);

Insert into ao_ss_bk_t5 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t5;

\d+ ao_ss_bk_t5

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t5';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_bk_t5');

Drop table ao_ss_bk_t5;


-- ========================================
-- Set the session level guc to the default value


SET gp_default_storage_options="appendonly=true,blocksize=32768";

show gp_default_storage_options;


-- Create table with no options
Drop table if exists ao_ss_bk_t1;
Create table ao_ss_bk_t1 ( i int, j int);

Insert into ao_ss_bk_t1 select i, i+1 from generate_series(1,10) i;

Select count(*) from ao_ss_bk_t1;

\d+ ao_ss_bk_t1;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t1';

Drop table ao_ss_bk_t1;



-- Create table with blocksize=1G

Drop table if exists ao_ss_bk_t2;
Create table ao_ss_bk_t2 ( i int, j int) with(blocksize=1048576);

Insert into ao_ss_bk_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t2;

\d+ ao_ss_bk_t2;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t2';

Drop table ao_ss_bk_t2;



-- Create table with appendonly=false

Drop table if exists ao_ss_bk_t3;
Create table ao_ss_bk_t3 ( i int, j int) with(appendonly=false);

Insert into ao_ss_bk_t3 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t3;

\d+ ao_ss_bk_t3;

select relkind, relstorage, reloptions from pg_class where relname='ao_ss_bk_t3';

Drop table ao_ss_bk_t3;


--- =====================================
-- GUC with invalid value, value without appendonly=true
SET gp_default_storage_options="appendonly=true,blocksize=9123";

-- GUC without appendonly=true specified
SET gp_default_storage_options="blocksize=8192";

show gp_default_storage_options;

-- With no option it will a heap table
Drop table if exists ao_ss_bk_t1;
Create table ao_ss_bk_t1( i int, j int);

Insert into ao_ss_bk_t1 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t1;

\d+ ao_ss_bk_t1;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_bk_t1';

Drop table ao_ss_bk_t1;
-- with appendonly=true specified the table will be created as AO table with blocksize

Drop table if exists ao_ss_bk_t2;
Create table ao_ss_bk_t2( i int, j int) with(appendonly=true);
Insert into ao_ss_bk_t2 select i, i+1 from generate_series(1,10) i;
Select count(*) from ao_ss_bk_t2;

\d+ ao_ss_bk_t2;

select relkind, relstorage,reloptions from pg_class where relname='ao_ss_bk_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ao_ss_bk_t2');

Drop table ao_ss_bk_t2;

-- Reset guc to default value

RESET gp_default_storage_options;
show gp_default_storage_options;
