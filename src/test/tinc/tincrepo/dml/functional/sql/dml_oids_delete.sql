-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description DELETE on table with OIDS
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(a) FROM dml_heap_r;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_heap_r ORDER BY 1;

DELETE FROM dml_heap_r WHERE a is NULL;

SELECT SUM(a) FROM dml_heap_r;
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_r WHERE tempoid.oid = dml_heap_r.oid AND tempoid.gp_segment_id = dml_heap_r.gp_segment_id AND tempoid.a is NOT NULL))foo;
