-- start_ignore
!\retcode  gpconfig -c autovacuum -v on;
!\retcode  gpstop -au;
-- end_ignore

-- Impose a stronger exit criteria for this test:
-- the AV launcher has shut down (and by extension the workers) following the config change.
-- This is done to ensure tests in the suite immediately following this one are run under the right conditions.
select check_autovacuum(true);
