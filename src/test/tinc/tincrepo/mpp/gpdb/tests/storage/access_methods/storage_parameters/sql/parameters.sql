-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Tests for various parameters setting at database role and session level

-- Session level

SET gp_default_storage_options='appendonly=true';
show gp_default_storage_options;

SET gp_default_storage_options='Appendonly=True';
show gp_default_storage_options;

SET gp_default_storage_options='appendonly-true';

SET gp_default_storage_options='appendonly=true-';

SET gp_default_storage_options='some_option=some_value';

SET gp_default_storage_options='appendonly   =true   ,orientation=   column  , checksum = false  ';

-- Database level

Alter database dsp_db1 set gp_default_storage_options='appendonly=true';
select datconfig from pg_database where datname='dsp_db1';

Alter database dsp_db1 set gp_default_storage_options='Appendonly=True';
select datconfig from pg_database where datname='dsp_db1';

Alter database dsp_db1 set gp_default_storage_options='appendonly-true';

Alter database dsp_db1 set gp_default_storage_options='appendonly=true-';

Alter database dsp_db1 set gp_default_storage_options='some_option=some_value';
select datconfig from pg_database where datname='dsp_db1';



-- Role level

Alter role dsp_role1 set gp_default_storage_options='appendonly=true';
select rolconfig from pg_roles where rolname='dsp_role1';

Alter role dsp_role1 set gp_default_storage_options='Appendonly=True';
select rolconfig from pg_roles where rolname='dsp_role1';

Alter role dsp_role1 set gp_default_storage_options='appendonly-true';

Alter role dsp_role1 set gp_default_storage_options='appendonly=true-';

Alter role dsp_role1 set gp_default_storage_options='some_option=some_value';
select rolconfig from pg_roles where rolname='dsp_role1';


-- Reset the values

Alter database dsp_db1 set gp_default_storage_options TO DEFAULT;
Alter role dsp_role1 set gp_default_storage_options TO DEFAULT;

select datconfig from pg_database where datname='dsp_db1';

select rolconfig from pg_roles where rolname='dsp_role1';
