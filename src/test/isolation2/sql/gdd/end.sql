include: helpers/server_helpers.sql;

ALTER SYSTEM RESET gp_enable_global_deadlock_detector;
ALTER SYSTEM RESET gp_global_deadlock_detector_period;

-- Use utility session on seg 0 to restart master. This way avoids the
-- situation where session issuing the restart doesn't disappear
-- itself.
1U:SELECT pg_ctl(dir, 'restart') from datadir;
-- Start new session on master to make sure it has fully completed
-- recovery and up and running again.
1: SHOW gp_enable_global_deadlock_detector;
1: SHOW gp_global_deadlock_detector_period;
