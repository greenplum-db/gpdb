-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLESPACE ct_ts1 filespace filerep_fs_z;
CREATE TABLESPACE ct_ts2 filespace filerep_fs_z;
CREATE TABLESPACE ct_ts3 filespace filerep_fs_z;
CREATE TABLESPACE ct_ts4 filespace filerep_fs_z;
CREATE TABLESPACE ct_ts5 filespace filerep_fs_z;

DROP TABLESPACE sync1_ts4;
DROP TABLESPACE ck_sync1_ts3;
DROP TABLESPACE ct_ts1;

