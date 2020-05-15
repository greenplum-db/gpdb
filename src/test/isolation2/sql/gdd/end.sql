-- disable GDD

-- start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v off;
! gpstop -rai;
-- end_ignore

1: SHOW gp_enable_global_deadlock_detector;
