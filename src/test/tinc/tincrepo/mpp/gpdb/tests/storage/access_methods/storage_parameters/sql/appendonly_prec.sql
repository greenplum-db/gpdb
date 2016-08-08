-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Database - Appendonly True
\c dsp_db1 
Drop table if exists dsp_ao_1;
Create table dsp_ao_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
Drop table dsp_ao_1;

-- Database - Appendonly True, Role - Appendonly False
\c dsp_db1 dsp_role1
Create table dsp_ao_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
Drop table dsp_ao_1;

-- Database - Appendonly True, Role - Appendonly False, Session - Appendonly True

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true';
Create table dsp_ao_1 ( i int, j int);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
Drop table dsp_ao_1;

-- Database - Appendonly True, Role - Appendonly False, Session - Appendonly True, Table Appendonly=False

\c dsp_db1 dsp_role1
SET gp_default_storage_options='appendonly=true';
Create table dsp_ao_1 ( i int, j int) with(appendonly=false);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
Drop table dsp_ao_1;

-- Database - Appendonly False, Role - Appendonly True, Session - Appendonly False, Table Appendonly=True

\c dsp_db2 dsp_role2
SET gp_default_storage_options='appendonly=false';
Create table dsp_ao_1 ( i int, j int) with(appendonly=true);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
Drop table dsp_ao_1;


