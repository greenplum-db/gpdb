-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create database ck_sync1_db1 template template0;
create database ck_sync1_db2 template template0;
create database ck_sync1_db3 template template0;
create database ck_sync1_db4 template template0;
create database ck_sync1_db5 template template0;
create database ck_sync1_db6 template template0;
create database ck_sync1_db7 template template0;

DROP DATABASE sync1_db2;
DROP DATABASE ck_sync1_db1;
