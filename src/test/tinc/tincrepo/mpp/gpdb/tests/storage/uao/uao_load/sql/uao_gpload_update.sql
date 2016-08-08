-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
SELECT * FROM customer;
SELECT * FROM gp_read_error_log('ext_customer');
