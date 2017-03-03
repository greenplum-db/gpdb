-- @gucs gp_create_table_random_default_distribution=off
create tablespace tbsp1  filespace filespace_a;
create tablespace tbsp2  filespace filespace_a;


-- Create table in tablespae
Create table tb_table1(i int, t text) with(appendonly=true) tablespace tbsp1;
Insert into tb_table1 values(generate_series(1,10), 'tablespace');

-- Create role
Create Resource Queue rsq_1 WITH (Active_Statements=30, Memory_Limit='1100MB', Max_Cost=1e+7, Priority=MEDIUM);
Create Resource Queue rsq_2 WITH (Active_Statements=30, Memory_Limit='1100MB', Max_Cost=1e+7, Priority=MEDIUM);
Create role bk_user1 LOGIN PASSWORD 'testpassword' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE CREATEEXTTABLE RESOURCE QUEUE rsq_1;

-- Create group
Create role bk_group1 LOGIN PASSWORD 'testpassword' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE CREATEEXTTABLE ROLE bk_user1 RESOURCE QUEUE rsq_1;
