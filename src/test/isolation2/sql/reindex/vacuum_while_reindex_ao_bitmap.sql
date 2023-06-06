DROP TABLE IF EXISTS reindex_ao;

CREATE TABLE reindex_ao (a INT) WITH (appendonly=true);
insert into reindex_ao select generate_series(1,1000);
insert into reindex_ao select generate_series(1,1000);
create index idx_bitmap_reindex_ao on reindex_ao USING bitmap(a);
-- @Description Ensures that a vacuum during reindex operations is ok
-- 

DELETE FROM reindex_ao WHERE a < 128;
1: BEGIN;
-- Remember index relfilenodes from coordinator and segments before
-- reindex.
1: create temp table old_relfilenodes as
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname = 'idx_bitmap_reindex_ao'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname = 'idx_bitmap_reindex_ao');
1: REINDEX index idx_bitmap_reindex_ao;
2&: VACUUM reindex_ao;
1: COMMIT;
2<:
-- Validate that reindex changed all index relfilenodes on coordinator as well as
-- segments.  The following query should return 0 tuples.
1: select oldrels.* from old_relfilenodes oldrels join
   (select gp_segment_id as dbid, relfilenode, relname from gp_dist_random('pg_class')
    where relname = 'idx_bitmap_reindex_ao'
    union all
    select gp_segment_id as dbid, relfilenode, relname from pg_class
    where relname = 'idx_bitmap_reindex_ao') newrels
    on oldrels.relfilenode = newrels.relfilenode
    and oldrels.dbid = newrels.dbid
    and oldrels.relname = newrels.relname;
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_ao WHERE a = 1500;
3: INSERT INTO reindex_ao VALUES (0);
