-- @Description Ensures that drop database during reindex operations on db does not work
-- 
1: @db_name reindexdb:commit;
1: REINDEX system reindexdb;
2: @db_name template1: drop database reindexdb;
1q:
2: drop database reindexdb;
