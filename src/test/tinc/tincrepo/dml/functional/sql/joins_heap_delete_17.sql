-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test17: Delete with sub-query
SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s WHERE a = (SELECT dml_heap_r.a FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a);
SELECT COUNT(*) FROM dml_heap_s;
