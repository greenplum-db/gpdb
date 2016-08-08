-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLESPACE sync2_ts1 filespace filerep_fs_z;
CREATE TABLESPACE sync2_ts2 filespace filerep_fs_z;

DROP TABLESPACE sync1_ts7;
DROP TABLESPACE ck_sync1_ts6;
DROP TABLESPACE ct_ts4;
DROP TABLESPACE resync_ts2;
DROP TABLESPACE sync2_ts1;

