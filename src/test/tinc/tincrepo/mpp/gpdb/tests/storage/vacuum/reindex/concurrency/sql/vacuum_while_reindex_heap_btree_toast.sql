-- @Description Ensures that a vacuum during reindex operations is ok
-- 

DELETE FROM reindex_toast_heap WHERE b % 4 = 0 ;
1: BEGIN;
-- Remember index relfilenodes from master and segments before
-- reindex.
1: create temp table old_relfilenodes as
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname = 'idx_btree_reindex_toast_heap'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname = 'idx_btree_reindex_toast_heap');
1: REINDEX index idx_btree_reindex_toast_heap;
2&: VACUUM reindex_toast_heap;
1: COMMIT;
2<:
-- Validate that reindex changed all index relfilenodes on master as well as
-- segments.  The following query should return 0 tuples.
1: select oldrels.* from old_relfilenodes oldrels join
   (select gp_segment_id as dbid, relfilenode, relname from gp_dist_random('pg_class')
    where relname = 'idx_btree_reindex_toast_heap'
    union all
    select gp_segment_id as dbid, relfilenode, relname from pg_class
    where relname = 'idx_btree_reindex_toast_heap') newrels
    on oldrels.relfilenode = newrels.relfilenode
    and oldrels.dbid = newrels.dbid
    and oldrels.relname = newrels.relname;
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_toast_heap WHERE a = '1500';
3: INSERT INTO reindex_toast_heap VALUES (0);
