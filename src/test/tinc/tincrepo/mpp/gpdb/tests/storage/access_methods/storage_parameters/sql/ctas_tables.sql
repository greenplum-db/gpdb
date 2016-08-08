-- 
-- @description CTAS tables with various guc setting


Alter role dsp_role1 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1';

-- At database level

-- Guc value appendonly=true
Alter database dsp_db1 set gp_default_storage_options='appendonly=true';

Select datconfig from pg_database where datname='dsp_db1';
Select rolconfig from pg_roles where rolname='dsp_role1';

\c dsp_db1

show gp_default_storage_options;


-- Create a CTAS table with no with clause options
Drop table if exists ao_db_ap_base;
Create table ao_db_ap_base( i int, j int);
Insert into ao_db_ap_base select i, i+1 from generate_series(1,10) i;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_base';

Drop table if exists ao_db_ap_ctas;
Create table ao_db_ap_ctas as select * from ao_db_ap_base;

\d+ ao_db_ap_ctas;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_ctas';

Drop table ao_db_ap_ctas;
Drop table ao_db_ap_base;

-- At role level

\c dsp_db1 dsp_role1

show gp_default_storage_options;

Drop table if exists ao_db_ap_base;
Create table ao_db_ap_base( i int, j int);
Insert into ao_db_ap_base select i, i+1 from generate_series(1,10) i;

Drop table if exists ao_db_ap_ctas;
Create table ao_db_ap_ctas as select * from ao_db_ap_base;

\d+ ao_db_ap_ctas;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_ctas';

Drop table ao_db_ap_ctas;


-- At session level

SET gp_default_storage_options='appendonly=true, compresstype=zlib, compresslevel=7';

show gp_default_storage_options;

Drop table if exists ao_db_ap_ctas;
Create table ao_db_ap_ctas as select * from ao_db_ap_base;

\d+ ao_db_ap_ctas;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_ctas';

Drop table ao_db_ap_ctas;

-- At table level

Drop table if exists ao_db_ap_ctas;
Create table ao_db_ap_ctas with( appendonly=true, orientation=column) as select * from ao_db_ap_base;

\d+ ao_db_ap_ctas;

select relkind, relstorage, reloptions from pg_class where relname='ao_db_ap_ctas';

Drop table ao_db_ap_ctas;

