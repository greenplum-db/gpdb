-- @gucs gp_create_table_random_default_distribution=off
-- Alter the table to new schema
Create Schema new_aoschema1;
Alter Table aoschema1.ao_table3 SET SCHEMA new_aoschema1;

-- Alter column Drop default
Alter table aoschema1.ao_table22 alter column stud_name drop default;

-- Alter column Drop default
Alter table public.ao_table20 alter column stud_name drop default;

-- Alter table inherit parent table
ALTER TABLE ao_table_child INHERIT ao_table_parent;

