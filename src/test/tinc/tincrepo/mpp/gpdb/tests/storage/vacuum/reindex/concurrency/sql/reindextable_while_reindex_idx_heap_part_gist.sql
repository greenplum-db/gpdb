-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
-- @Description Ensures that a reindex table during reindex index operations is ok
-- 

DELETE FROM reindex_crtab_part_heap_gist  WHERE id < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX TABLE  reindex_crtab_part_heap_gist;
2&: reindex index idx_reindex_crtab_part_heap_gist_1_prt_de_fault;
1: COMMIT;
2<:
2: COMMIT;
3: select count(*) from reindex_crtab_part_heap_gist where id = 998;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_crtab_part_heap_gist where id = 999;
3: SELECT 1 AS relfilenode_same_on_all_segs_maintable from gp_dist_random('pg_class') WHERE relname = 'reindex_crtab_part_heap_gist' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

3: select 1 AS relfilenode_same_on_all_segs_mainidx from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_crtab_part_heap_gist' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

3: select  1 AS relfilenode_same_on_all_segs_partition_default_data from gp_dist_random('pg_class') WHERE relname = 'reindex_crtab_part_heap_gist_1_prt_de_fault' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

3: select  1 AS relfilenode_same_on_all_segs_partition_default_idx from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_crtab_part_heap_gist_1_prt_de_fault' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

3: select 1 AS relfilenode_same_on_all_segs_partition_1_data from gp_dist_random('pg_class') WHERE relname = 'reindex_crtab_part_heap_gist_1_prt_p_one' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

3: select 1 AS relfilenode_same_on_all_segs_partition_1_idx from gp_dist_random('pg_class') WHERE relname = 'idx_reindex_crtab_part_heap_gist_1_prt_p_one' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
