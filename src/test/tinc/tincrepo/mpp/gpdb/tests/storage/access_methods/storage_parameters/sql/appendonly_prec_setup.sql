-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
Alter database dsp_db1 set gp_default_storage_options='appendonly=true';
Alter role dsp_role1 set gp_default_storage_options='appendonly=false';

Alter database dsp_db2 set gp_default_storage_options='appendonly=false';
Alter role dsp_role2 set gp_default_storage_options='appendonly=true';

Select datname,datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2');

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2');
