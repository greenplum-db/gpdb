-- @Description Ensures that a reindex table during reindex index operations is ok
-- 

 DELETE FROM reindex_crtab_aoco_gist WHERE id = 301;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_crtab_aoco_gist;
2&: REINDEX TABLE  reindex_crtab_aoco_gist;
1: COMMIT;
2<:
2: COMMIT;
3: select count(*) from reindex_crtab_aoco_gist where  (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: set enable_bitmapscan=true;
3: select count(*) from reindex_crtab_aoco_gist where (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_aoco_gist' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'reindex_crtab_aoco_gist' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

