-- Before running test, set statement_timeout to 1 minute so that even if some
-- error make 'gp_wait_parallel_retrieve_cursor()' hang, it will be canceled
-- when timeout.

!\retcode gpconfig -c statement_timeout -v 60000;
!\retcode gpstop -u;

