-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_alter_ao_bitmap;

CREATE TABLE reindex_alter_ao_bitmap (a INT,j INT) WITH (appendonly=true);
insert INTO reindex_alter_ao_bitmap SELECT i, i+10 from generate_series(1,1000)i;
insert INTO reindex_alter_ao_bitmap SELECT i, i+10 from generate_series(1,1000)i;
create INDEX idx_reindex_alter_ao_bitmap ON reindex_alter_ao_bitmap USING bitmap(a);
SELECT 1 AS relfilenode_same_on_all_segs FROM gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_alter_ao_bitmap' GROUP BY relfilenode HAVING count(*) = (SELECT count(*) FROM     gp_segment_configuration WHERE role='p' AND content > -1);

