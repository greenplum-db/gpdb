-- @gucs gp_create_table_random_default_distribution=off
-- Rename tablename
Alter Table co_table2 RENAME TO co_table_new_name;

-- Alter the table to new schema
Create Schema new_coschema1;
Alter Table coschema1.co_table3 SET SCHEMA new_coschema1;

-- Alter table add column
Alter Table co_table1 ADD COLUMN added_col character varying(30) default 'default';

-- Alter table rename column
Alter Table co_table4 RENAME COLUMN before_rename_col TO after_rename_col;

-- Alter table Drop column
Alter table co_table12 Drop column a_ts_without;

-- Alter table column type
Alter Table co_table5 ALTER COLUMN change_datatype_col TYPE int4;

-- Alter column set default expression
Alter Table co_table6 ALTER COLUMN col_set_default SET DEFAULT 0;

-- Alter column Drop default
Alter table co_table21 alter column stud_name drop default;

-- Alter column drop NOT NULL
Alter Table co_table7 ALTER COLUMN int_col DROP NOT NULL;

-- Alter column set NOT NULL
Alter Table co_table8 ALTER COLUMN numeric_col SET NOT NULL;

-- Alter table SET STORAGE
Alter Table co_table9 ALTER char_vary_col SET STORAGE PLAIN;

-- Alter table inherit parent table
ALTER TABLE co_table_child INHERIT co_table_parent;
