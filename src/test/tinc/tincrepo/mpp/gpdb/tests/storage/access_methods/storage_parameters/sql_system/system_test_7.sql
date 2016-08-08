-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- System level Guc 'orientation=column,compresstype=zlib,compresslevel=6'

-- Database level Guc 'appendonly=true,compresstype=quicklz,checksum=false'

-- Role level Guc 'appendonly=true, compresstype=zlib, checksum=true'

-- Session level Guc 'appendonly=true,orientation=column,compresstype=rle_type'

Alter database dsp_db1 set gp_default_storage_options='appendonly=true,compresstype=quicklz,checksum=false';

Alter role dsp_role1 set gp_default_storage_options='appendonly=true, compresstype=zlib, checksum=true';

\c dsp_db1 dsp_role1

SET gp_default_storage_options='appendonly=true,orientation=column,compresstype=rle_type';

show gp_default_storage_options;

-- Create table with no options

Drop table if exists ds_sy_t1;
Create table ds_sy_t1 (i int, j int);
Create index ds_sy_t1_ix on ds_sy_t1(i);

Insert into ds_sy_t1 select i, i+1 from generate_series(1,10) i;

update ds_sy_t1 set j=i where i>5;

Delete from ds_sy_t1 where i<2;

Select count(*) from ds_sy_t1;

\d+ ds_sy_t1;

select relstorage, reloptions from pg_class where relname='ds_sy_t1';

Drop table ds_sy_t1;

-- Create table with table level options

Drop table if exists ds_sy_t2;
Create table ds_sy_t2 (i int, j int) with(appendonly=true,compresstype=zlib, compresslevel=6, checksum=false, blocksize=8192);

Insert into ds_sy_t2 select i, i+1 from generate_series(1,10) i;

Select count(*) from ds_sy_t2;

\d+ ds_sy_t2;

select relstorage, reloptions from pg_class where relname='ds_sy_t2';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid = (select oid from pg_class where relname='ds_sy_t2');

Drop table ds_sy_t2;
