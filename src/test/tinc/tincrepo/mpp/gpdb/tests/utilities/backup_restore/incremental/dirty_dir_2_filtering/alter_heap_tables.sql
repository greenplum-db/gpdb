-- @gucs gp_create_table_random_default_distribution=off
-- Rename tablename
ALTER TABLE heap_table1 RENAME TO heap_table_new_name;

-- Alter the table to new schema
CREATE SCHEMA new_schema1;
ALTER TABLE schema1.heap_table2 SET SCHEMA new_schema1;

