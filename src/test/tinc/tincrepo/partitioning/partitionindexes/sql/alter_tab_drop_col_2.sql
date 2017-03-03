-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description ALTER TABLE, Drop column
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col4,col2);

ALTER TABLE pt_lt_tab DROP column col4;
SELECT * FROM pt_lt_tab WHERE col2 > 41 ORDER BY col2,col3 LIMIT 5;
