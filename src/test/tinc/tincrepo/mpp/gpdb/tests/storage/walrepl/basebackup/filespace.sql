-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

CREATE TABLESPACE ts FILESPACE fs_walrepl_a;

CREATE TABLE fs_table(a int, b int) TABLESPACE ts;
