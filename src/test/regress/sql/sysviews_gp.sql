-- Test the Greenplum 'gp_%' system views

-- Select the 'pg_%' system views that do not have a 'gp_%' counterpart.
-- This serves as a boundary-checking test for any missing 'pg_%' system view
-- that we are not aware of. If you find any view that fails this check, please
-- examine if we need to add it to the src/backend/catalog/system_views_gp.in
-- and also add a test case in this file.
SELECT table_name FROM information_schema.views 
WHERE table_schema = 'pg_catalog' 
AND table_name LIKE 'pg\_%' 
AND REPLACE(table_name, 'pg_', 'gp_') NOT IN
(
  SELECT table_name FROM information_schema.views 
  WHERE table_schema = 'pg_catalog'
);

-- check each gp_ view created in system_views_gp.sql
select count(*) >= 0 from gp_config;
select count(*) >= 0 from gp_cursors;
select count(*) >= 0 from gp_file_settings;
select count(*) >= 0 from gp_replication_origin_status;
select count(*) >= 0 from gp_replication_slots;
select count(*) >= 0 from gp_settings;
select count(*) >= 0 from gp_stat_activity;
select count(*) >= 0 from gp_stat_all_indexes;
select count(*) >= 0 from gp_stat_all_tables;
select count(*) >= 0 from gp_stat_archiver;
select count(*) >= 0 from gp_stat_bgwriter;
select count(*) >= 0 from gp_stat_database;
select count(*) >= 0 from gp_stat_database_conflicts;
select count(*) >= 0 from gp_stat_gssapi;
select count(*) >= 0 from gp_stat_progress_analyze;
select count(*) >= 0 from gp_stat_progress_basebackup;
select count(*) >= 0 from gp_stat_progress_cluster;
select count(*) >= 0 from gp_stat_progress_copy;
select count(*) >= 0 from gp_stat_progress_create_index;
select count(*) >= 0 from gp_stat_progress_vacuum;
select count(*) >= 0 from gp_stat_slru;
select count(*) >= 0 from gp_stat_ssl;
select count(*) >= 0 from gp_stat_subscription;
select count(*) >= 0 from gp_stat_sys_indexes;
select count(*) >= 0 from gp_stat_sys_tables;
select count(*) >= 0 from gp_stat_user_functions;
select count(*) >= 0 from gp_stat_user_indexes;
select count(*) >= 0 from gp_stat_user_tables;
select count(*) >= 0 from gp_stat_wal;
select count(*) >= 0 from gp_stat_wal_receiver;
select count(*) >= 0 from gp_stat_xact_all_tables;
select count(*) >= 0 from gp_stat_xact_sys_tables;
select count(*) >= 0 from gp_stat_xact_user_functions;
select count(*) >= 0 from gp_stat_xact_user_tables;
select count(*) >= 0 from gp_statio_all_indexes;
select count(*) >= 0 from gp_statio_all_sequences;
select count(*) >= 0 from gp_statio_all_tables;
select count(*) >= 0 from gp_statio_sys_indexes;
select count(*) >= 0 from gp_statio_sys_sequences;
select count(*) >= 0 from gp_statio_sys_tables;
select count(*) >= 0 from gp_statio_user_indexes;
select count(*) >= 0 from gp_statio_user_sequences;
select count(*) >= 0 from gp_statio_user_tables;
select count(*) >= 0 from gp_stats;
select count(*) >= 0 from gp_stats_ext;
