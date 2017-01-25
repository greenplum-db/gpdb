-- @gucs gp_create_table_random_default_distribution=off
-- Alter table set to a new tablespace
Alter table tb_table1 set tablespace tbsp2;

-- Alter role to a new resource queue
Alter role bk_user1 resource queue rsq_2;
