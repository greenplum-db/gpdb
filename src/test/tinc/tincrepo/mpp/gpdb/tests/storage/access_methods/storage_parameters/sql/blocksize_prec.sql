-- 
-- @description Precedence testing with GUC values at database, role, session, table level


-- Database - blocksize=8192

\c dsp_db1 
Drop table if exists dsp_bk_1;
Create table dsp_bk_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_bk_1';
Drop table dsp_bk_1;


-- Database - blocksize=8192 ; Role - blocksize=65536

\c dsp_db1 dsp_role1
Create table dsp_bk_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_bk_1';
Drop table dsp_bk_1;

-- Database - blocksize=8192, Role - blocksize=65536, Session - blocksize=1048576

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, blocksize=1048576';
Create table dsp_bk_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_bk_1';
Drop table dsp_bk_1;


-- Database - blocksize=8192, Role - blocksize=65536, Session - blocksize=1048576, Table - blocksize=32768

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, blocksize=1048576';
Create table dsp_bk_1 ( i int, j int) with (blocksize=32768);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_bk_1';
Drop table dsp_bk_1;
