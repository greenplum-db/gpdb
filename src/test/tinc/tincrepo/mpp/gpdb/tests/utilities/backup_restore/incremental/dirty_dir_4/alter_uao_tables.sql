-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

-- Update one of the row inserted after a truncate
Update uao_table9 set char_vary_col = '8_eight' where bigint_col=5;

-- Update one of the row updated after a truncate on partition table
Update uao_part03 set d=10;

--Update with no where cluase : Some partitions no data 
Update uao_part09 set amt=45.3;

--Delete with no where cluase (Some partitions with no data) 
Delete from uao_part10;

--Delete with no where clause 
Delete from uao_part08;

