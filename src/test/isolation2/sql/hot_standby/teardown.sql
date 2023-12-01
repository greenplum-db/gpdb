-- reset the setup for hot standby tests
!\retcode gpconfig -r hot_standby;
!\retcode gpconfig -r synchronous_commit;
!\retcode gpstop -ar;
