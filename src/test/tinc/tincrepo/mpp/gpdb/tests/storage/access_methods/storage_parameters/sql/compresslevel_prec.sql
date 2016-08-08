-- 
-- @description Precedence testing with GUC values at database, role, session, table level


-- Database - Orientation=column,Compresslevel=1

\c dsp_db1 
Drop table if exists dsp_cl_1;
Create table dsp_cl_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_1';
Drop table dsp_cl_1;


-- Database - Orientation=column,Compresslevel=1; Role - Orientation=column,Compresslevel=3 

\c dsp_db1 dsp_role1
Create table dsp_cl_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_1';
Drop table dsp_cl_1;

-- Database - Orientation=column,Compresslevel=1; Role - Orientation=column,Compresslevel=3; 
-- Session - Orientation=column,Compresslevel=4

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, orientation=column,compresslevel=4';
Create table dsp_cl_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_1';
Drop table dsp_cl_1;


-- Database - Orientation=column,Compresslevel=1; Role - Orientation=column,Compresslevel=3; 
-- Session - Orientation=column,Compresslevel=4;Table - Orientation=column,Compresslevel=9

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, orientation=column,compresslevel=4';
Create table dsp_cl_1 ( i int, j int) with (compresslevel=9);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_1';
Drop table dsp_cl_1;


-- Database - Orientation=row,Compresslevel=2; Role - Orientation=row, Compresslevel=4
\c dsp_db2 dsp_role2
Drop table if exists dsp_cl_2;
Create table dsp_cl_2 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_2';
Drop table dsp_cl_2;

-- Database - Orientation=row,Compresslevel=2; Role - Orientation=row, Compresslevel=4;
-- Session - Orientation=row,Compresslevel=3

\c dsp_db2 dsp_role2
SET gp_default_storage_options='appendonly=true, orientation=row,compresslevel=3';
Create table dsp_cl_2 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_2';
Drop table dsp_cl_2;

-- Database - Orientation=row,Compresslevel=2; Role - Orientation=row, Compresslevel=4; 
-- Session - Orientation=row,Compresslevel=3, Table - Orientation=row,Compresslevel=5

\c dsp_db2 dsp_role2
SET gp_default_storage_options='appendonly=true, orientation=row,compresslevel=3';
Create table dsp_cl_2 ( i int, j int) with(orientation=row,compresslevel=5);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_2';
Drop table dsp_cl_2;

-- Database - Orientation=row,Compresstype=quicklz,compresslevel=1; Role - Orientation=column, compresstype=quicklz,compresslevel=1; 
-- Session - Orientation=row,Compresstype=quicklz,compresslevel=1, Table - compresslevel=0

\c dsp_db3 dsp_role3
SET gp_default_storage_options='appendonly=true, orientation=row,compresstype=quicklz, compresslevel=1';
Create table dsp_cl_2 ( i int, j int) with(compresstype=none,compresslevel=0);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_2';
Drop table dsp_cl_2;


-- Database - Orientation=column,Compresstype=rle_type,Compresslevel=1; Role - Orientation=column, compresstype=rle_type,Compresslevel=3; 
-- Session - Orientation=column,Compresstype=rle_type,Compresslevel=4, Table - Orientation=column,Compresstype=rle_type,Compresslevel=2

\c dsp_db4 dsp_role4
SET gp_default_storage_options='appendonly=true, orientation=column,compresstype=rle_type, compresslevel=4';
Create table dsp_cl_2 ( i int, j int) with(compresstype=rle_type,compresslevel=2);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_cl_2';
Drop table dsp_cl_2;

