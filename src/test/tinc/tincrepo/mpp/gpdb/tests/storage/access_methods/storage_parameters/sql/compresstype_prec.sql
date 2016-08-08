-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

-- Database - Orientation=column,Compresstype=quicklz

\c dsp_db1 
Drop table if exists dsp_ct_1;
Create table dsp_ct_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_1';
Drop table dsp_ct_1;


-- Database - Orientation=column,Compresstype=quicklz; Role - Orientation=column, compresstype=zlib 

\c dsp_db1 dsp_role1
Create table dsp_ct_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_1';
Drop table dsp_ct_1;

-- Database - Orientation=column,Compresstype=quicklz; Role - Orientation=column, compresstype=zlib; 
-- Session - Orientation=column,Compresstype=rle_type

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, orientation=column,compresstype=rle_type';
Create table dsp_ct_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_1';
Drop table dsp_ct_1;


-- Database - Orientation=column,Compresstype=quicklz; Role - Orientation=column, compresstype=zlib; 
-- Session - Orientation=column,Compresstype=rle_type, Table - Orientation=column,Compresstype=quicklz

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, orientation=column,compresstype=rle_type';
Create table dsp_ct_1 ( i int, j int) with(compresstype=quicklz);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_1';
Drop table dsp_ct_1;

-- Database - Orientation=row,Compresstype=quicklz

\c dsp_db2 dsp_role4
Drop table if exists dsp_ct_2;
Create table dsp_ct_2 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_2';
Drop table dsp_ct_2;

-- Database - Orientation=row,Compresstype=quicklz; Role - Orientation=row, compresstype=zlib 

\c dsp_db2 dsp_role2
Drop table if exists dsp_ct_2;
Create table dsp_ct_2 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_2';
Drop table dsp_ct_2;

-- Database - Orientation=row,Compresstype=quicklz; Role - Orientation=row, compresstype=zlib; 
-- Session - Orientation=column,Compresstype=rle_type

\c dsp_db2 dsp_role2
SET gp_default_storage_options='appendonly=true, orientation=column,compresstype=rle_type';
Drop table if exists dsp_ct_2;
Create table dsp_ct_2 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_2';
Drop table dsp_ct_2;

-- Database - Orientation=row,Compresstype=quicklz; Role - Orientation=row, compresstype=zlib; 
-- Session - Orientation=column,Compresstype=rle_type, Table - Orientation=row,Compresstype=quicklz

\c dsp_db2 dsp_role2
SET gp_default_storage_options='appendonly=true, orientation=column,compresstype=rle_type';
Drop table if exists dsp_ct_2;
Create table dsp_ct_2 ( i int, j int) with(orientation=row,compresstype=quicklz);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_2';
Drop table dsp_ct_2;

-- Database - Orientation=row,Compresstype=zlib; Role - Orientation=column, compresstype=rle_type; 
-- Session - Orientation=row,Compresstype=quicklz, Table - Orientation=column,Compresstype=none

\c dsp_db3 dsp_role3
SET gp_default_storage_options='appendonly=true, orientation=row,compresstype=zlib';
Drop table if exists dsp_ct_2;
Create table dsp_ct_2 ( i int, j int) with(orientation=column,compresstype=none);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ct_2';
Drop table dsp_ct_2;
