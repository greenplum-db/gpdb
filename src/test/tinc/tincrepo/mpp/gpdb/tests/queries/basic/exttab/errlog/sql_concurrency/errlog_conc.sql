-- @concurrency 5
-- Execute this concurrently to write to error logs concurrently

SELECT COUNT(*) FROM exttab_conc_1, exttab_conc_2;
