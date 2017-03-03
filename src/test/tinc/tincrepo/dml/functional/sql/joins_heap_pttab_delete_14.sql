-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Delete on table with composite distcol
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_p;
DELETE FROM dml_heap_pt_p USING dml_heap_pt_r WHERE dml_heap_pt_p.b::int = dml_heap_pt_r.b::int and dml_heap_pt_p.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_p;
