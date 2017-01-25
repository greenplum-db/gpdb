-- @gucs gp_create_table_random_default_distribution=off
-- Alter table set distributed byt to a new column
Alter Table co_table1 SET distributed by(text_col);

--Alter table set distributed randomly
Alter Table new_coschema1.co_table3 SET distributed randomly;

-- Alter table set Reorganize to true 
Alter Table co_table10 SET WITH (reorganize=true);;

-- Alter table set Reorganize to false 
Alter Table co_table11 SET WITH (reorganize=false);;

-- Alter table add table constraint
ALTER TABLE co_table13  ADD CONSTRAINT lenchk CHECK (char_length(char_vary_col) < 10);

-- Alter table drop constriant
Alter table co_table_new_name  Drop constraint tbl_chk_con1;

-- Alter column  set statistics
Alter table co_table14 alter column before_rename_col set statistics 3;

-- Alter table SET without cluster
Alter table co_table15 set without cluster;
Alter table co_table16 set without cluster;

-- Alter table NO inherit from parent table
Alter Table co_table_child NO INHERIT co_table_parent;

--Alter table to a new owner
Alter Table co_table18 Owner to co_user1;

