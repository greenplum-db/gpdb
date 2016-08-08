-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
set gp_role = utility;

select gp_activate_standby();
