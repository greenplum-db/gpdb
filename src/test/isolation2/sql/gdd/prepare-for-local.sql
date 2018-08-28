-- gdd can also detect local deadlocks, however it might break at different
-- node with the local deadlock detecotr.  To make the local deadlock testcases
-- stable we reset the gdd period to 2min so should not be triggered during
-- the local deadlock tests.

-- start_ignore
! gpconfig -r gp_global_deadlock_detector_period;
! gpstop -u;
-- end_ignore

-- the new setting need some time to be loaded
SELECT pg_sleep(2);

SHOW gp_global_deadlock_detector_period;
