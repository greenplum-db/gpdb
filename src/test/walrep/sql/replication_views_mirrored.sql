-- Check how many WAL replication mirrors are up
SELECT count(*) FROM gp_segment_configuration where preferred_role='m' and status='u';

-- Check how many WAL replication mirrors are up and in-sync
SELECT count(*) FROM gp_segment_configuration where preferred_role='m' and mode='s';

-- Check pg_stat_replication view (this assumes standby master has not been created)
SELECT * FROM pg_stat_replication;

-- Check pg_stat_replication view of master and primary segments
SELECT gp_segment_id, application_name, state, sync_state FROM gp_stat_replication;
