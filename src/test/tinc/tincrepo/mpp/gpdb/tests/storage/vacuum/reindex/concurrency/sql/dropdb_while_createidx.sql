-- @Description Ensures that drop database during drop index operations does not works (blocks) 
-- 
1: @db_name reindexdb:BEGIN;
1: CREATE INDEX reindex_tab_with_dropdb_idx on reindex_tab_with_dropdb(b);
1: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'reindex_tab_with_dropdb_idx' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
2: @db_name template1: drop database reindexdb;
1: COMMIT;
1q:
2: drop database reindexdb;
