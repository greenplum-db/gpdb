-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLESPACE ck_sync1_ts1 filespace filerep_fs_z;
CREATE TABLESPACE ck_sync1_ts2 filespace filerep_fs_z;
CREATE TABLESPACE ck_sync1_ts3 filespace filerep_fs_z;
CREATE TABLESPACE ck_sync1_ts4 filespace filerep_fs_z;
CREATE TABLESPACE ck_sync1_ts5 filespace filerep_fs_z;
CREATE TABLESPACE ck_sync1_ts6 filespace filerep_fs_z;
CREATE TABLESPACE ck_sync1_ts7 filespace filerep_fs_z;

DROP TABLESPACE sync1_ts2;
DROP TABLESPACE ck_sync1_ts1;

