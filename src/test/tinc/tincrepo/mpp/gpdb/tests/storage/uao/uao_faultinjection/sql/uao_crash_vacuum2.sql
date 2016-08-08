-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
set gp_select_invisible = true;
SELECT * FROM foo;
set gp_select_invisible = false;
