-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Database - Appendonly True, , blocksize=8192, Role checksum=true, Session=compresslevel=4, table compresstype=zlib


\c dsp_db1 dsp_role1
SET gp_default_storage_options='compresslevel=4';
Create table dsp_ao_1 ( i int, j int) with(compresstype=zlib, appendonly=true);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
select compresslevel, compresstype, blocksize, checksum, columnstore from pg_appendonly where relid ='dsp_ao_1'::regclass;
Drop table dsp_ao_1;

-- Database - Appendonly True, Orientation column; Role -compresslevel=5 , Session- blocksize=8192, Table Appendonly=false

\c dsp_db2 dsp_role2
SET gp_default_storage_options='blocksize=8192';
Create table dsp_ao_1 ( i int, j int) with(appendonly=false);
Select relkind, relstorage, reloptions from pg_class where relname='dsp_ao_1';
Drop table dsp_ao_1;


