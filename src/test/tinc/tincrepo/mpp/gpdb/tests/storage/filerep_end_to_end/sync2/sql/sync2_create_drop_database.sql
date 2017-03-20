-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create database sync2_db1 template template0;
create database sync2_db2 template template0;

DROP DATABASE sync1_db7;
DROP DATABASE ck_sync1_db6;
DROP DATABASE ct_db4;
DROP DATABASE resync_db2;
DROP DATABASE sync2_db1;
