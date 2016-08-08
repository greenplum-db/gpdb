-- @Description Ensures that a truncate during reindex operations on GiST index is ok
-- 

DELETE FROM reindex_heap_gist  WHERE id < 128;
1: BEGIN;
1: REINDEX index idx_gist_reindex_heap;
2&: TRUNCATE TABLE reindex_heap_gist;
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_heap' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
