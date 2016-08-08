-- @Description Ensures that reindex system during reindex system on same db works
-- 
1: @db_name reindexdb:COMMIT;
1: REINDEX system reindexdb;
2: @db_name reindexdb:COMMIT;
2: reindex system reindexdb;
