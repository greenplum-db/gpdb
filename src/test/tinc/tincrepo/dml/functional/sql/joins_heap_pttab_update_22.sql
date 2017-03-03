-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @skip MPP-19148
-- @description update_test22: Update on table wit composite distribution key
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_p SET a = dml_heap_pt_p.b % 2 FROM dml_heap_pt_r WHERE dml_heap_pt_p.b::int = dml_heap_pt_r.b::int and dml_heap_pt_p.a = dml_heap_pt_r.a;
SELECT SUM(a) FROM dml_heap_pt_r;
