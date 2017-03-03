-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description ALTER TABLE, Unique index with Primary Key, unique index on the default partition 
ALTER TABLE pt_lt_tab ADD unique(col2,col1);
SELECT * FROM pt_lt_tab  WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;
