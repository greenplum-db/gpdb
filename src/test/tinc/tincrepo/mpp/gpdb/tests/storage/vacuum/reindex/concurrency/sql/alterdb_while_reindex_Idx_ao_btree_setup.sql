-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
1: DROP DATABASE IF EXISTS reindexdb;
1: DROP DATABASE IF EXISTS reindexdb2;
1: create database reindexdb template template0;
2:@db_name reindexdb: DROP TABLE IF EXISTS reindex_alter_ao_btree;
2: CREATE TABLE reindex_alter_ao_btree (a INT,j INT) WITH (appendonly=true);
2: insert INTO reindex_alter_ao_btree SELECT i, i+10 from generate_series(1,1000)i;
2: insert INTO reindex_alter_ao_btree SELECT i, i+10 from generate_series(1,1000)i;
2: CREATE INDEX idx_reindex_alter_ao_btree on reindex_alter_ao_btree(j);
2: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_alter_ao_btree' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM     gp_segment_configuration WHERE role='p' AND content > -1);
