-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Set default to database, role

Alter database dsp_db1 set gp_default_storage_options to default;
Select datname, datconfig from pg_database where datname='dsp_db1';

Alter role dsp_role1 set gp_default_storage_options to default;
Select rolname, rolconfig from pg_roles where rolname='dsp_role1';
