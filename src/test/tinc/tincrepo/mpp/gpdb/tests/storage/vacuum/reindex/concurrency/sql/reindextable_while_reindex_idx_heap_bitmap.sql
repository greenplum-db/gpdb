-- @Description Ensures that a reindex table during reindex index operations is ok
-- 

DELETE FROM reindex_crtab_heap_bitmap WHERE a < 128;
1: BEGIN;
1: create temp table old_relfilenodes as
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname = 'idx_reindex_crtab_heap_bitmap'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname = 'idx_reindex_crtab_heap_bitmap');
2: BEGIN;
1: REINDEX index idx_reindex_crtab_heap_bitmap;
2&: REINDEX TABLE  reindex_crtab_heap_bitmap;
1: COMMIT;
2<:
-- Session 2 has not committed yet.  Session 1 should see effects of
-- its own reindex command above in pg_class.  The following query
-- validates that reindex command in session 1 indeed generates new
-- relfilenode for the index.
1: insert into old_relfilenodes
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname = 'idx_reindex_crtab_heap_bitmap'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname = 'idx_reindex_crtab_heap_bitmap');
-- Expect two distinct relfilenodes per segment in old_relfilenodes table.
1: select distinct count(distinct relfilenode), relname from old_relfilenodes group by dbid, relname;
2: COMMIT;
-- After session 2 commits, the relfilenode it assigned to the index
-- is visible to session 1.
1: insert into old_relfilenodes
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname = 'idx_reindex_crtab_heap_bitmap'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname = 'idx_reindex_crtab_heap_bitmap');
-- Expect three distinct relfilenodes per segment in old_relfilenodes table.
1: select distinct count(distinct relfilenode), relname from old_relfilenodes group by dbid, relname;

3: select count(*) from reindex_crtab_heap_bitmap where a = 1000;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_crtab_heap_bitmap where a = 1000;
