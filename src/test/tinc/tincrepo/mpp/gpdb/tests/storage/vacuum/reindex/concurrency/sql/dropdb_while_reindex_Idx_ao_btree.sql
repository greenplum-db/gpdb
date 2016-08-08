-- @Description Ensures that drop database during reindex operations does not work
-- 
1: @db_name reindexdb:DELETE FROM reindex_alter_ao_btree WHERE a < 128;
2: @db_name reindexdb:BEGIN;
2: REINDEX index idx_reindex_alter_ao_btree;
2: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_alter_ao_btree' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: @db_name template1: drop database reindexdb;
2: COMMIT;
1q:
2q:
3: @db_name template1: drop database reindexdb;
