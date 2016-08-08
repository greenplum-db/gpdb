-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
TRUNCATE TABLE cr_co_truncate_table;
SELECT COUNT(*) FROM cr_co_truncate_table;
DROP TABLE cr_co_truncate_table;
