DROP TABLE IF EXISTS reindex_crtab_heap_bitmap;

CREATE TABLE reindex_crtab_heap_bitmap (a INT); 
insert into reindex_crtab_heap_bitmap select generate_series(1,1000);
insert into reindex_crtab_heap_bitmap select generate_series(1,1000);
create index idx_reindex_crtab_heap_bitmap on reindex_crtab_heap_bitmap(a);
select 1 as oid_same_on_all_segs from gp_dist_random('pg_class')   where relname = 'idx_reindex_crtab_heap_bitmap' group by oid having count(*) = (select count(*) from gp_segment_configuration where role='p' and content > -1);

-- @Description Ensures that a create index during reindex operations is ok
-- 

DELETE FROM reindex_crtab_heap_bitmap WHERE a < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_crtab_heap_bitmap;
2: create index idx_reindex_crtab_heap_bitmap2 on reindex_crtab_heap_bitmap(a);
1: COMMIT;
2: COMMIT;
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_heap_bitmap' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_crtab_heap_bitmap2' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
