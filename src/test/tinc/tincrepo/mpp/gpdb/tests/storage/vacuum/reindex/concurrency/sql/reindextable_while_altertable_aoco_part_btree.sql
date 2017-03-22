-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
-- @Description Ensures that a reindex table during alter tab drop col operations is ok
-- 

DELETE FROM reindex_crtabforalter_part_aoco_btree  WHERE id < 12;
1: BEGIN;
2: BEGIN;
1: REINDEX TABLE  reindex_crtabforalter_part_aoco_btree;
2&: alter table reindex_crtabforalter_part_aoco_btree drop column amt;
1: COMMIT;
2<:
2: COMMIT;
3: select count(*) from reindex_crtabforalter_part_aoco_btree where id = 29;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_crtabforalter_part_aoco_btree where id = 29;

3: select c_relname, 1 as different_relfilenode from before_reindex_crtabforalter_part_aoco_btree b where exists (select oid, gp_segment_id, relfilenode from gp_dist_random('pg_class') where relname like 'idx_reindex_crtabforalter_part_aoco_btree%' and b.c_oid = oid and b.c_gp_segment_id = gp_segment_id and b.c_relfilenode != relfilenode) group by b.c_oid, b.c_relname;

3: select count(*) from pg_attribute where attname = 'amt' and attrelid = (select oid from pg_class where relname = 'reindex_crtabforalter_part_aoco_btree');
