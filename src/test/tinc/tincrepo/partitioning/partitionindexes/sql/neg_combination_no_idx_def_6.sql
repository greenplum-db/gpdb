-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests Combination tests ,index exists on some regular partitions and not on the default partition
SELECT * FROM pt_lt_tab WHERE col2 is NULL ORDER BY col2,col3 LIMIT 5; 
