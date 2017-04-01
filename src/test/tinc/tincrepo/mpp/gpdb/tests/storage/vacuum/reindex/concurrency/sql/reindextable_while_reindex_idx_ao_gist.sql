-- @Description Ensures that a reindex table during reindex index operations is ok
-- 

DELETE FROM reindex_crtab_ao_gist WHERE id = 301;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_crtab_ao_gist;
2&: REINDEX TABLE  reindex_crtab_ao_gist;
1: COMMIT;
2<:
2: COMMIT;
3: select count(*) from reindex_crtab_ao_gist where  (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: set enable_bitmapscan=true;
3: select count(*) from reindex_crtab_ao_gist where (box1 * POINT '(2.0, 0)' ) ~= (BOX '( (1,1), (2,2) )' * POINT '(2.0, 0)');

-- expect to have all the segments update relfilenode twice, one by 'reindex index', and one by 'reindex table'
-- by exposing the invisible rows, we can see the historical updates to the relfilenode of given index
-- aggregate by gp_segment_id and oid can verify total number of updates
-- finally compare total number of segments + master to ensure all segments and master got reindexed twice
3: set gp_select_invisible=on;
3: select sum(relfilenode_updated_twice)::int/(select count(*) from gp_segment_configuration where role='p' and xmax=0) as all_segs_reindexed_twice from (select (count(*)/3) as relfilenode_updated_twice from (select gp_segment_id, oid, * from pg_class union all select gp_segment_id, oid, * from gp_dist_random('pg_class')) all_pg_class where relname = 'idx_reindex_crtab_ao_gist' group by gp_segment_id, oid) per_seg_filenode_updated;
3: set gp_select_invisible=off;
