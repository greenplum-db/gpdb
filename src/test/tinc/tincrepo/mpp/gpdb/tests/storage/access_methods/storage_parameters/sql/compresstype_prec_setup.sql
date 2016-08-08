-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
Alter database dsp_db1 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=quicklz';
Alter role dsp_role1 set gp_default_storage_options='appendonly=true, orientation=row, compresstype=zlib';

Alter database dsp_db2 set gp_default_storage_options='appendonly=true,orientation=row, compresstype=quicklz';
Alter role dsp_role2 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=zlib';

Alter database dsp_db3 set gp_default_storage_options='appendonly=true,orientation=row, compresstype=zlib';
Alter role dsp_role3 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=rle_type';

Alter role dsp_role4 reset gp_default_storage_options;

Select datname,datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3');

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4');
