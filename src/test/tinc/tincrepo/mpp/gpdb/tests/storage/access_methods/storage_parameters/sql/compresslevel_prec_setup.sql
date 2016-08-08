-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
Alter database dsp_db1 set gp_default_storage_options='appendonly=true, orientation=column, compresslevel=1';
Alter role dsp_role1 set gp_default_storage_options='appendonly=true, orientation=column, compresslevel=3';

Alter database dsp_db2 set gp_default_storage_options='appendonly=true,orientation=row, compresslevel=2';
Alter role dsp_role2 set gp_default_storage_options='appendonly=true, orientation=row, compresslevel=4';

Alter database dsp_db3 set gp_default_storage_options='appendonly=true,orientation=row,compresstype=quicklz,compresslevel=1';
Alter role dsp_role3 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=quicklz,compresslevel=1';

Alter database dsp_db4 set gp_default_storage_options='appendonly=true,orientation=column,compresstype=rle_type,compresslevel=1';
Alter role dsp_role4 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=rle_type,compresslevel=3';

Select datname,datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4');

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4', 'dsp_role4');
