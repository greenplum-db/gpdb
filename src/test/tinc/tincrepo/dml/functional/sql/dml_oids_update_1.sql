-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description UPDATE to constant value on table with OIDS
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,a FROM dml_heap_r ORDER BY 1;

UPDATE dml_heap_r SET a = 1;

SELECT SUM(a) FROM dml_heap_r;
-- The query checks that the tuple oids remain the remain pre and post update .
-- SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1 is a join on the tuple oids before update and after update. If the oids remain the same the below query should return 1 row which is equivalent to the number of rows in the table
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.col1 = dml_heap_r.col1))foo;

