-- @Description Ensures that drop database during reindex operations does not work
-- 
1: @db_name reindexdb:DELETE FROM reindex_alter_ao_btree WHERE a < 128;
2: @db_name reindexdb:BEGIN;
2: REINDEX index idx_reindex_alter_ao_btree;
3: @db_name template1: alter database reindexdb rename to reindexdb2;
2: COMMIT;
