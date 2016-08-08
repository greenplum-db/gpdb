-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_alter_heap_btree;

CREATE TABLE reindex_alter_heap_btree (a INT, j INT); 
insert INTO reindex_alter_heap_btree SELECT i, i+10 from generate_series(1,1000)i;
insert INTO reindex_alter_heap_btree SELECT i, i+10 from generate_series(1,1000)i;
create index idx_reindex_alter_heap_btree on reindex_alter_heap_btree(j);
SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_alter_heap_btree' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM     gp_segment_configuration WHERE role='p' AND content > -1);

