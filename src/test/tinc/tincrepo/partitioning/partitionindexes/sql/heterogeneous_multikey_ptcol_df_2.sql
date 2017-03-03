-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
