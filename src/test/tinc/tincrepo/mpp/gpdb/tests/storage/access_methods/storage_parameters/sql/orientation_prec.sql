-- 
-- @description Precedence testing with GUC values at database, role, session, table level

-- Database - Orientation=column
\c dsp_db1 
Drop table if exists dsp_or_1;
Create table dsp_or_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_or_1';
Drop table dsp_or_1;

-- Database - Orientation=column, Role - Orientation=row 
\c dsp_db1 dsp_role1
Create table dsp_or_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_or_1';
Drop table dsp_or_1;

-- Database - Orientation=column, Role - Orientation=row , Session - Orientation=column 

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, orientation=column';
Create table dsp_or_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_or_1';
Drop table dsp_or_1;

-- Database - Orientation=column, Role - Orientation=row , Session - Orientation=column , Table=row

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, orientation=column';
Create table dsp_or_1 ( i int, j int) with(orientation=row);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_or_1';
Drop table dsp_or_1;

-- Database - Orientation=row, Role - Orientation=column , Session - Orientation=row , Table=column

\c dsp_db2 dsp_role2
SET gp_default_storage_options='appendonly=true, orientation=row';
Create table dsp_or_1 ( i int, j int) with(orientation=column);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_or_1';
Drop table dsp_or_1;


