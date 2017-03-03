-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests ALTER TABLE, drop index
-- start_ignore
DROP INDEX IF EXISTS idx1_1_prt_part4;
-- end_ignore
SELECT * FROM pt_lt_tab WHERE col2 = 35 ORDER BY col2,col3 LIMIT 5;
