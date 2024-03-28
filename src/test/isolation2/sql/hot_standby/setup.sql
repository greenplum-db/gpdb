-- setup for hot standby tests
!\retcode gpconfig -c hot_standby -v on;
-- let primary wait for standby to apply changes, make test less flaky
!\retcode gpconfig -c synchronous_commit -v remote_apply;
-- make it faster to handle query conflict
!\retcode gpconfig -c max_standby_streaming_delay -v 1000;
-- disable autovacuum, to not affect the manual VACUUM in the tests
!\retcode gpconfig -c autovacuum -v off;
!\retcode gpstop -ar;
