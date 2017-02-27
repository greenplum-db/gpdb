-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test9:  With UNION/INTERSECT/EXCEPT in the predicate condition( 0 rows )
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT NULL EXCEPT SELECT NULL)::int; 
SELECT COUNT(*) FROM dml_union_r;

