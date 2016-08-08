-- @Description Ensures that a vacuum during reindex operations is ok
-- 

DELETE FROM reindex_ao WHERE a < 128;
1: BEGIN;
1: REINDEX index idx_btree_reindex_ao;
2&: VACUUM ANALYZE reindex_ao;
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_ao WHERE a = 1500;
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_btree_reindex_ao' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 as reltuples_same_as_count from pg_class where relname = 'reindex_ao'  and reltuples = (SELECT count(*) from reindex_ao);
3: INSERT INTO reindex_ao VALUES (0);
