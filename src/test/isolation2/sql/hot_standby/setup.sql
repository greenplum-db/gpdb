-- setup for hot standby tests
!\retcode gpconfig -c hot_standby -v on;
-- let primary wait for standby to apply changes, make test less flaky
!\retcode gpconfig -c synchronous_commit -v remote_apply;
!\retcode gpstop -ar;
