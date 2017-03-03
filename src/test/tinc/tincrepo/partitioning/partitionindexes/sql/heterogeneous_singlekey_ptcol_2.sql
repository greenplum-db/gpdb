-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index, index on partition key, b-tree index on all partitions
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
SELECT * FROM pt_lt_tab WHERE col2 between 1 AND 50 ORDER BY col2,col3 LIMIT 5;
