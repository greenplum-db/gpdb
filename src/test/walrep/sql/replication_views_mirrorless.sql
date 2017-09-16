-- Check how many WAL replication mirrors are up and synced
SELECT count(*) FROM gp_segment_configuration where preferred_role='m' and mode='s';

-- Check each pg_stat_replication view (this assumes standby master has not been created)
SELECT * FROM pg_stat_replication;
SELECT gp_segment_id, application_name, state, sync_state FROM pg_stat_replication_segments;
SELECT gp_segment_id, application_name, state, sync_state FROM pg_stat_replication_all;
