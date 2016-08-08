-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Reset the gucs

Alter database dsp_db1 set gp_default_storage_options to default;
Alter role dsp_role1 set gp_default_storage_options to default;

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2', 'dsp_db3', 'dsp_db4') order by datname;
