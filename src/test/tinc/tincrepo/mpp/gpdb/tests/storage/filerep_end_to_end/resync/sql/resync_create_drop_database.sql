-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create database resync_db1 template template0;
create database resync_db2 template template0;
create database resync_db3 template template0;

DROP DATABASE sync1_db6;
DROP DATABASE ck_sync1_db5;
DROP DATABASE ct_db3;
DROP DATABASE resync_db1;
