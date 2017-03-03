-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests combination tests, Index exists on some continuous set of partitions, e.g. p1,p2,p3
SELECT * FROM pt_lt_tab WHERE col2 = 35 ORDER BY col2,col3 LIMIT 5;

