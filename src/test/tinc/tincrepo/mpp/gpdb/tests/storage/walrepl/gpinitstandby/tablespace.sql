-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE fs_walrepl_table(a int, b int) TABLESPACE ts_walrepl_a;
