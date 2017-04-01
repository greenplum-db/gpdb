-- @Description Ensures that a create index during reindex operations on GiST index is ok
-- 

DELETE FROM reindex_heap_gist  WHERE id < 128;
1: BEGIN;
1: REINDEX index idx_gist_reindex_heap;
2: create index idx_gist_reindex_heap2 on reindex_heap_gist USING Gist(target);
1: COMMIT;
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;
3: select count(*) from reindex_heap_gist;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_heap_gist;

3: insert into reindex_heap_gist (id, owner, description, property, poli, target) values(1500, 'gpadmin', 'Reindex Concurrency test', '((1500, 1500), (1560, 1580))', '( (111, 112), (114, 115), (110, 110) )', '( (96, 86), 96)' );
3:  SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_heap' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_heap2' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
