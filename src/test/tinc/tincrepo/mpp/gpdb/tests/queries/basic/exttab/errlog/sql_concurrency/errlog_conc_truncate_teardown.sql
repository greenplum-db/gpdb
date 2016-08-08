-- Should not return any rows
SELECT gp_read_error_log('exttab_conc_truncate_1');
SELECT gp_read_error_log('exttab_conc_truncate_2');
