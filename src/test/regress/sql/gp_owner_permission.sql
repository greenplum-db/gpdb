-- @Description This is to test the reindex functionality of a database. 
-- A user who owns a database should be able to reindex the database and all the tables in it
-- even if he does not own them and has no direct privileges on them.
DROP database IF EXISTS reindexdb2;

DROP role IF EXISTS test1;
DROP role IF EXISTS test2;
CREATE ROLE test1 WITH login;
CREATE ROLE test2 WITH login CREATEDB;
SET role = test2;
CREATE database reindexdb2;

\c reindexdb2
BEGIN;
SET role = test1;
CREATE TABLE mytab1_heap(a int, b int);
CREATE INDEX idx_mytab1_heap ON mytab1_heap(b);
INSERT INTO mytab1_heap SELECT a , a - 10 FROM generate_series(1,100) a;
DELETE FROM mytab1_heap WHERE a % 4 = 0;
REVOKE ALL PRIVILEGES ON mytab1_heap FROM test2;
CREATE TABLE mytab1_ao(a int, b int) WITH (appendonly = TRUE);
CREATE INDEX idx_mytab1_ao ON mytab1_ao(b);
INSERT INTO mytab1_ao SELECT a , a - 10 FROM generate_series(1,100) a;
DELETE FROM mytab1_ao WHERE a % 4 = 0;
REVOKE ALL PRIVILEGES ON mytab1_ao FROM test2;
CREATE TABLE mytab1_aoco(a int, b int) WITH (appendonly = TRUE, orientation = COLUMN);
CREATE INDEX idx_mytab1_aoco ON mytab1_aoco(b);
INSERT INTO mytab1_aoco SELECT a , a - 10 FROM generate_series(1,100) a;
DELETE FROM mytab1_aoco WHERE a % 4 = 0;
REVOKE ALL PRIVILEGES ON mytab1_aoco FROM test2;
COMMIT;

SET role = test2;
SET client_min_messages=WARNING;
REINDEX DATABASE  reindexdb2;
select usename as user_for_reindex_heap from pg_stat_operations where objname = 'idx_mytab1_heap' and subtype = 'REINDEX' ;
select usename as user_for_reindex_ao from pg_stat_operations where objname = 'idx_mytab1_ao' and subtype = 'REINDEX' ;
select usename as user_for_reindex_aoco from pg_stat_operations where objname = 'idx_mytab1_aoco' and subtype = 'REINDEX' ;
SELECT 1 AS relfilenode_same_on_all_segs_heap from gp_dist_random('pg_class')   WHERE relname = 'idx_mytab1_heap' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
SELECT 1 AS relfilenode_same_on_all_segs_ao from gp_dist_random('pg_class')   WHERE relname = 'idx_mytab1_ao' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
SELECT 1 AS relfilenode_same_on_all_segs_aoco from gp_dist_random('pg_class')   WHERE relname = 'idx_mytab1_aoco' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
