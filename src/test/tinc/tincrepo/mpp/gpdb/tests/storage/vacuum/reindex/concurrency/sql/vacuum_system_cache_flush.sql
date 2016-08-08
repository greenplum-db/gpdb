-- @Description Ensures that a vacuum during reindex operations is ok with guc gp_test_system_cache_flush_force set to plain
-- 

DELETE FROM reindex_heap WHERE a < 128;
1: BEGIN;
1: set gp_test_system_cache_flush_force = plain;
1: REINDEX index idx_btree_reindex_heap;
2&: VACUUM reindex_heap;
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_heap WHERE a >= 128 ;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: SELECT COUNT(*) FROM reindex_heap WHERE a >= 128 ;
3: INSERT INTO reindex_heap VALUES (0);
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_btree_reindex_heap' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
