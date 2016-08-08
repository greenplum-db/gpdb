-- @Description Ensures that a vacuum during reindex operations is ok
-- 

DELETE FROM reindex_toast_heap WHERE b % 4 = 0 ;
1: BEGIN;
1: REINDEX index idx_btree_reindex_toast_heap;
2&: VACUUM reindex_toast_heap;
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_toast_heap WHERE a = 1500;
3: INSERT INTO reindex_toast_heap VALUES (0);
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_btree_reindex_toast_heap' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
