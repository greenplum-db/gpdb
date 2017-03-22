-- 
-- @description Precedence testing with GUC values at database, role, session, table level

-- Database - checksum=true

\c dsp_db1 
Drop table if exists dsp_ck_1;
Create table dsp_ck_1 ( i int, j int);
select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='dsp_ck_1';
Drop table dsp_ck_1;


-- Database - checksum=true ; Role - checksum=false

\c dsp_db1 dsp_role1
Create table dsp_ck_1 ( i int, j int);
select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='dsp_ck_1';
Drop table dsp_ck_1;

-- Database - checksum=true ; Role - checksum=false, Session - checksum=true

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, checksum=true';
Create table dsp_ck_1 ( i int, j int);
select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='dsp_ck_1';
Drop table dsp_ck_1;


-- Database - checksum=true ; Role - checksum=false, Session - checksum=true, Table - checksum=false

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true, checksum=true';
Create table dsp_ck_1 ( i int, j int) with (checksum=false);
select relstorage, reloptions,checksum from pg_class c , pg_appendonly a where c.oid=a.relid and c.relname='dsp_ck_1';
Drop table dsp_ck_1;
