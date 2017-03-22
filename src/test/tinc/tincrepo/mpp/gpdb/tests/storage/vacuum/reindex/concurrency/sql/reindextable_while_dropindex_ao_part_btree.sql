-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
-- @Description Ensures that a reindex table during reindex index operations is ok
-- 

DELETE FROM reindex_dropindex_crtab_part_ao_btree  WHERE id < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX TABLE  reindex_dropindex_crtab_part_ao_btree;
2&: DROP INDEX idx_reindex_dropindex_crtab_part_owner_ao_btree;
1: COMMIT;
2<:
2: COMMIT;
3: select count(*) from reindex_dropindex_crtab_part_ao_btree where id = 998;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_dropindex_crtab_part_ao_btree where id = 999;

3: select c_relname, 1 as different_relfilenode from before_reindex_dropindex_crtab_part_ao_btree b where exists (select oid, gp_segment_id, relfilenode from gp_dist_random('pg_class') where relname like 'idx_reindex_dropindex_crtab_part_ao_btree%' and b.c_oid = oid and b.c_gp_segment_id = gp_segment_id and b.c_relfilenode != relfilenode) group by b.c_oid, b.c_relname;

-- expect only the index on parent table to be dropped; drop index will not drop indexes on child partitions
3: select 1-count(1) as index_dropped from (select * from pg_class union all select * from gp_dist_random('pg_class')) t where t.relname = 'idx_reindex_dropindex_crtab_part_owner_ao_btree';
