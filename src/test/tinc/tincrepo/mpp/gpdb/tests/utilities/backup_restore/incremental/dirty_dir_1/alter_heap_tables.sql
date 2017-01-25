-- @gucs gp_create_table_random_default_distribution=off
-- Rename tablename
ALTER TABLE heap_table1 RENAME TO heap_table_new_name;

-- Alter the table to new schema
CREATE SCHEMA new_schema1;
ALTER TABLE schema1.heap_table2 SET SCHEMA new_schema1;

-- Alter table add column
ALTER TABLE heap_table3 ADD COLUMN added_col character varying(30);

-- Alter table rename column
ALTER TABLE heap_table_new_name RENAME COLUMN col_numeric TO after_rename_col;

-- Alter table column type
ALTER TABLE heap_table3 ALTER COLUMN change_datatype_col TYPE int4;

-- Alter column set default expression
ALTER TABLE heap_table3 ALTER COLUMN col_set_default SET DEFAULT 0;

-- Alter column drop NOT NULL
ALTER TABLE heap_table3 ALTER COLUMN int_col DROP NOT NULL;

-- Alter column set NOT NULL
ALTER TABLE heap_table3 ALTER COLUMN numeric_col SET NOT NULL;

-- Alter table SET STORAGE
ALTER TABLE heap_table3 ALTER char_vary_col SET STORAGE PLAIN;

