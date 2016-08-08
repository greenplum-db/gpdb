-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
SELECT COUNT(*) FROM cr_co_truncate_table;
DROP TABLE cr_co_truncate_table;
