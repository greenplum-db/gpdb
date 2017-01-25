-- @gucs gp_create_table_random_default_distribution=off
-- Drop gobal objects and dependednt objects

Drop table tb_table1;
Drop tablespace tbsp1;
Drop tablespace tbsp2;

-- Drop roles and resource_queues
Drop role bk_user1;
Drop role bk_group1;
Drop resource queue rsq_1;
Drop resource queue rsq_2;
