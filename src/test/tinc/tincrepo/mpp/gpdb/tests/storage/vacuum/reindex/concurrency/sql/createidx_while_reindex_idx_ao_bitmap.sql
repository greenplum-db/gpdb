-- @Description Ensures that a create index during reindex operations is ok
-- 

DELETE FROM reindex_crtab_ao_bitmap WHERE a < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_crtab_ao_bitmap;
2: create index idx_reindex_crtab_ao_bitmap2 on reindex_crtab_ao_bitmap USING bitmap(a);
1: COMMIT;
2: COMMIT;
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_ao_bitmap' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_ao_bitmap2' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
