-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Reset guc to default value for all the roles

Alter role dsp_role1 set gp_default_storage_options TO DEFAULT;
Alter role dsp_role2 set gp_default_storage_options TO DEFAULT;
Alter role dsp_role3 set gp_default_storage_options TO DEFAULT;
Alter role dsp_role4 set gp_default_storage_options TO DEFAULT;

select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2', 'dsp_role3', 'dsp_role4') order by rolname;
