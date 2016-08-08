-- @Description This is to test the reindex functionality of a database. 
-- A user who ownes a database should be able to reindex the database and all the tables in it  
-- even if he does not own them or has no privileges on them.
-- 
-- 
DROP database IF EXISTS reindexdb2;
1: DROP role IF EXISTS test1;
1: DROP role IF EXISTS test2;
1: CREATE ROLE test1 WITH login;
1: CREATE ROLE test2 WITH login CREATEDB;
1: SET role = test2;
1: CREATE database reindexdb2;
2: @db_name reindexdb2: BEGIN;
2: SET role = test1;
2: CREATE TABLE mytab1_heap(a int, b int);
2: CREATE INDEX idx_mytab1_heap ON mytab1_heap(b);
2: INSERT INTO mytab1_heap SELECT a , a - 10 FROM generate_series(1,100) a;
2: DELETE FROM mytab1_heap WHERE a % 4 = 0;
2: REVOKE ALL PRIVILEGES ON mytab1_heap FROM test2;
2: CREATE TABLE mytab1_ao(a int, b int) WITH (appendonly = TRUE);
2: CREATE INDEX idx_mytab1_ao ON mytab1_ao(b);
2: INSERT INTO mytab1_ao SELECT a , a - 10 FROM generate_series(1,100) a;
2: DELETE FROM mytab1_ao WHERE a % 4 = 0;
2: REVOKE ALL PRIVILEGES ON mytab1_ao FROM test2;
2: CREATE TABLE mytab1_aoco(a int, b int) WITH (appendonly = TRUE, orientation = COLUMN);
2: CREATE INDEX idx_mytab1_aoco ON mytab1_aoco(b);
2: INSERT INTO mytab1_aoco SELECT a , a - 10 FROM generate_series(1,100) a;
2: DELETE FROM mytab1_aoco WHERE a % 4 = 0;
2: REVOKE ALL PRIVILEGES ON mytab1_aoco FROM test2;
2: COMMIT;
3: @db_name reindexdb2: SET role = test2;
3: SET role = test2;
3: REINDEX DATABASE  reindexdb2;
3: select usename as user_for_reindex_heap from pg_stat_operations where objname = 'idx_mytab1_heap' and subtype = 'REINDEX' ;
3: select usename as user_for_reindex_ao from pg_stat_operations where objname = 'idx_mytab1_ao' and subtype = 'REINDEX' ;
3: select usename as user_for_reindex_aoco from pg_stat_operations where objname = 'idx_mytab1_aoco' and subtype = 'REINDEX' ;
3: SELECT 1 AS relfilenode_same_on_all_segs_heap from gp_dist_random('pg_class')   WHERE relname = 'idx_mytab1_heap' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS relfilenode_same_on_all_segs_ao from gp_dist_random('pg_class')   WHERE relname = 'idx_mytab1_ao' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS relfilenode_same_on_all_segs_aoco from gp_dist_random('pg_class')   WHERE relname = 'idx_mytab1_aoco' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
