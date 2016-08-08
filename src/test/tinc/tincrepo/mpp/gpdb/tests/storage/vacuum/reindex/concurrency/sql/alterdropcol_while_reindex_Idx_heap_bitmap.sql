-- @Description Ensures that alter table during reindex operations is ok
-- 

DELETE FROM reindex_alter_heap_bitmap WHERE a < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX INDEX idx_reindex_alter_heap_bitmap;
2&: ALTER TABLE reindex_alter_heap_bitmap drop COLUMN j;
1: COMMIT;
2<:
2: COMMIT;
3: INSERT INTO reindex_alter_heap_bitmap VALUES(2500);
3: SELECT count(*) FROM reindex_alter_heap_bitmap where a = 2500;
3: SELECT 1 AS relfilenode_same_on_all_segs FROM gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_alter_heap_bitmap' GROUP BY relfilenode HAVING count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
