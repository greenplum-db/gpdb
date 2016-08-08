-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLESPACE resync_ts1 filespace filerep_fs_z;
CREATE TABLESPACE resync_ts2 filespace filerep_fs_z;
CREATE TABLESPACE resync_ts3 filespace filerep_fs_z;

DROP TABLESPACE sync1_ts6;
DROP TABLESPACE ck_sync1_ts5;
DROP TABLESPACE ct_ts3;
DROP TABLESPACE resync_ts1;

