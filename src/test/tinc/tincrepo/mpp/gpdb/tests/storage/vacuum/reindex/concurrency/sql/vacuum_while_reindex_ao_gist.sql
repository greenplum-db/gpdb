-- @Description Ensures that a vacuum during reindex operations on GiST index is ok
-- 

DELETE FROM reindex_ao_gist  WHERE id < 128;
1: BEGIN;
1: REINDEX index idx_gist_reindex_ao;
2&: VACUUM reindex_ao_gist;
1: COMMIT;
2<:
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_ao_gist WHERE id = 1500;
3: insert into reindex_ao_gist (id, owner, description, property, poli, target) values(1500, 'gpadmin', 'Reindex Concurrency test', '((1500, 1500), (1560, 1580))', '( (111, 112), (114, 115), (110, 110) )', '( (96, 86), 96)' );
3:  SELECT COUNT(*) FROM reindex_ao_gist WHERE id = 1500;
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_ao' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
