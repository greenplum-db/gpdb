-- @Description Ensures that a drop table during reindex operations is ok
-- 

DELETE FROM reindex_drop_heap_bitmap WHERE a < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_drop_heap_bitmap;
2&: drop table reindex_drop_heap_bitmap;
1: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_drop_heap_bitmap' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM pg_class where relname = 'reindex_drop_heap_bitmap';
