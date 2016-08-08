-- @Description Ensures that reindex database during reindex database on same db works
-- 
1: @db_name reindexdb:COMMIT;
1: REINDEX database reindexdb;
2: @db_name reindexdb:COMMIT;
2: reindex database reindexdb;
