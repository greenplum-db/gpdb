-- @concurrency 5

-- Try executing gp_truncate_error_log concurrently

SELECT gp_truncate_error_log('exttab_conc_truncate_1');
SELECT gp_truncate_error_log('exttab_conc_truncate_2');