-- @Description Ensures that a vacuum during reindex operations on GiST index is ok
-- 

DELETE FROM reindex_heap_gist  WHERE id < 128;
1: BEGIN;
1: REINDEX index idx_gist_reindex_vacuum_heap;
2&: VACUUM reindex_heap_gist;
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;
3: insert into reindex_heap_gist (id, owner, description, property, poli, target) values(1500, 'gpadmin', 'Reindex Concurrency test', '((1500, 1500), (1560, 1580))', '( (111, 112), (114, 115), (110, 110) )', '( (96, 86), 96)' );
3:  SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;

-- expect to have all the segments update relfilenode by reindex
-- by exposing the invisible rows, we can see the historical updates to the relfilenode of given index
-- aggregate by gp_segment_id and oid can verify total number of updates
-- finally compare total number of segments + master to ensure all segments and master got reindexed
3: set gp_select_invisible=on;
3: select relname, sum(relfilenode_updated_count)::int/(select count(*) from gp_segment_configuration where role='p' and xmax=0) as all_segs_reindexed_count from (select oid, relname, (count(relfilenode)-1) as relfilenode_updated_count from (select gp_segment_id, oid, relfilenode, relname from pg_class union all select gp_segment_id, oid, relfilenode, relname from gp_dist_random('pg_class')) all_pg_class where relname like 'idx_gist_reindex_vacuum_heap%' group by gp_segment_id, oid, relname) per_seg_filenode_updated group by oid, relname;
3: set gp_select_invisible=off;
