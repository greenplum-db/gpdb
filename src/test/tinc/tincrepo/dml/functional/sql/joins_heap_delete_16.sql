-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test16: Delete with PREPARE plan
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a)foo;
SELECT COUNT(*) FROM dml_heap_s;
PREPARE plan_del_2 as DELETE FROM dml_heap_s WHERE b in (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a);
EXECUTE plan_del_2;
SELECT COUNT(*) FROM dml_heap_s;
