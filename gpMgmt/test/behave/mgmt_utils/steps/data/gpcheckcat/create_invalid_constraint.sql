set allow_system_table_mods=true;

-- Create a table with a distribution key that's a superset of its primary key
create table foo(i int primary key);
update pg_constraint set conkey='{}' where conname = 'foo_pkey';

-- Force a table with unique constraints to have a random distribution
create table bar(i int unique) distributed by (i);
update gp_distribution_policy set distkey='', distclass='' where localoid='bar'::regclass;
