-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP DATABASE IF EXISTS reindexdb;
CREATE DATABASE reindexdb;
1: @db_name reindexdb:BEGIN;
1: drop table if exists reindex_tab_with_dropdb;
1: create table reindex_tab_with_dropdb (a int , b int) with (appendonly=true, orientation=column);
1: insert into reindex_tab_with_dropdb select i, i -10 from generate_series(1,100) i;
1: commit;
