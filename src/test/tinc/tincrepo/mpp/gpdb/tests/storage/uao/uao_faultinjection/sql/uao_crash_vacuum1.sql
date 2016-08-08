-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DELETE FROM foo where a <= 5;
vacuum foo;
