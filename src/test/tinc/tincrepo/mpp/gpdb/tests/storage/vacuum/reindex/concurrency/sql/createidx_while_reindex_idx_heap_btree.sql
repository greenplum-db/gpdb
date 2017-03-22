-- @Description Ensures that a create table during reindex operations is ok
-- 

DELETE FROM reindex_crtab_heap_btree WHERE a < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_crtab_heap_btree;
2: create index idx_reindex_crtab_heap_btree2 on reindex_crtab_heap_btree(a);
1: COMMIT;
2: COMMIT;
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_heap_btree' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_heap_btree2' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
