-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @ description Negative tests Combination tests, Index exists on some regular partitions and on the default partition [INDEX exists on non-consecutive partitions, e.g. p1,p3,p5]
SELECT * FROM pt_lt_tab WHERE col2 > 15 ORDER BY col2,col3 LIMIT 5;
