-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- cleanup, DROP DATABASE
--

DROP DATABASE sync1_db8;
DROP DATABASE ck_sync1_db7;
DROP DATABASE ct_db5;
DROP DATABASE resync_db3;
DROP DATABASE sync2_db2;
DROP DATABASE ck_sync1_db2;
DROP DATABASE ck_sync1_db4;
DROP DATABASE ct_db2;
DROP DATABASE sync1_db3;
DROP DATABASE sync1_db5;

\c template1;
DROP DATABASE gptest;

--
-- cleanup, DROP TABLESPACE
--

DROP TABLESPACE sync1_ts8;
DROP TABLESPACE sync1_ts3;
DROP TABLESPACE sync1_ts5;
DROP TABLESPACE ck_sync1_ts2;
DROP TABLESPACE ck_sync1_ts4;
DROP TABLESPACE ck_sync1_ts7;
DROP TABLESPACE ct_ts2;
DROP TABLESPACE ct_ts5;
DROP TABLESPACE resync_ts3;
DROP TABLESPACE sync2_ts2;


--
-- DROP TABLESPACE
--

DROP TABLESPACE sync1_ts_a1 ;
DROP TABLESPACE sync1_ts_a2 ;
DROP TABLESPACE sync1_ts_a3 ;
DROP TABLESPACE sync1_ts_a4 ;
DROP TABLESPACE sync1_ts_a5 ;
DROP TABLESPACE sync1_ts_a6 ;
DROP TABLESPACE sync1_ts_a7 ;
DROP TABLESPACE sync1_ts_a8 ;

DROP TABLESPACE sync1_ts_b1 ;
DROP TABLESPACE sync1_ts_b2 ;
DROP TABLESPACE sync1_ts_b3 ;
DROP TABLESPACE sync1_ts_b4 ;
DROP TABLESPACE sync1_ts_b5 ;
DROP TABLESPACE sync1_ts_b6 ;
DROP TABLESPACE sync1_ts_b7 ;
DROP TABLESPACE sync1_ts_b8 ;

DROP TABLESPACE sync1_ts_c1 ;
DROP TABLESPACE sync1_ts_c2 ;
DROP TABLESPACE sync1_ts_c3 ;
DROP TABLESPACE sync1_ts_c4 ;
DROP TABLESPACE sync1_ts_c5 ;
DROP TABLESPACE sync1_ts_c6 ;
DROP TABLESPACE sync1_ts_c7 ;
DROP TABLESPACE sync1_ts_c8 ;


DROP TABLESPACE ck_sync1_ts_a1 ;
DROP TABLESPACE ck_sync1_ts_a2 ;
DROP TABLESPACE ck_sync1_ts_a3 ;
DROP TABLESPACE ck_sync1_ts_a4 ;
DROP TABLESPACE ck_sync1_ts_a5 ;
DROP TABLESPACE ck_sync1_ts_a6 ;
DROP TABLESPACE ck_sync1_ts_a7 ;


DROP TABLESPACE ck_sync1_ts_b1 ;
DROP TABLESPACE ck_sync1_ts_b2 ;
DROP TABLESPACE ck_sync1_ts_b3 ;
DROP TABLESPACE ck_sync1_ts_b4 ;
DROP TABLESPACE ck_sync1_ts_b5 ;
DROP TABLESPACE ck_sync1_ts_b6 ;
DROP TABLESPACE ck_sync1_ts_b7 ;


DROP TABLESPACE ck_sync1_ts_c1 ;
DROP TABLESPACE ck_sync1_ts_c2 ;
DROP TABLESPACE ck_sync1_ts_c3 ;
DROP TABLESPACE ck_sync1_ts_c4 ;
DROP TABLESPACE ck_sync1_ts_c5 ;
DROP TABLESPACE ck_sync1_ts_c6 ;
DROP TABLESPACE ck_sync1_ts_c7 ;


DROP TABLESPACE ct_ts_a1 ;
DROP TABLESPACE ct_ts_a2 ;
DROP TABLESPACE ct_ts_a3 ;
DROP TABLESPACE ct_ts_a4 ;
DROP TABLESPACE ct_ts_a5 ;


DROP TABLESPACE ct_ts_b1 ;
DROP TABLESPACE ct_ts_b2 ;
DROP TABLESPACE ct_ts_b3 ;
DROP TABLESPACE ct_ts_b4 ;
DROP TABLESPACE ct_ts_b5 ;


DROP TABLESPACE ct_ts_c1 ;
DROP TABLESPACE ct_ts_c2 ;
DROP TABLESPACE ct_ts_c3 ;
DROP TABLESPACE ct_ts_c4 ;
DROP TABLESPACE ct_ts_c5 ;


DROP TABLESPACE resync_ts_a1 ;
DROP TABLESPACE resync_ts_a2 ;
DROP TABLESPACE resync_ts_a3 ;


DROP TABLESPACE resync_ts_b1 ;
DROP TABLESPACE resync_ts_b2 ;
DROP TABLESPACE resync_ts_b3 ;


DROP TABLESPACE resync_ts_c1 ;
DROP TABLESPACE resync_ts_c2 ;
DROP TABLESPACE resync_ts_c3 ;

DROP TABLESPACE sync2_ts_a1 ;
DROP TABLESPACE sync2_ts_a2 ;


DROP TABLESPACE sync2_ts_b1 ;
DROP TABLESPACE sync2_ts_b2 ;


DROP TABLESPACE sync2_ts_c1 ;
DROP TABLESPACE sync2_ts_c2 ;

--
--DROP FILESPACE
--

DROP FILESPACE filerep_fs_a;
DROP FILESPACE filerep_fs_b;
DROP FILESPACE filerep_fs_c;
DROP FILESPACE sync1_fs_1;
DROP FILESPACE  filerep_fs_z;

--
-- Create gptest which is used as PGDATABASE for CI
--
create database gptest template template0;
