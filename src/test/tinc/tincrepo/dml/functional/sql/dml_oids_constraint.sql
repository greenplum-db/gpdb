-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @gpopt 1.532
-- @description DML on table with constraints and OIDS(Negative Test)
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
INSERT INTO dml_heap_check_r SELECT i, i ,'r', i FROM generate_series(1,2)i;
SELECT SUM(a),SUM(b) FROM dml_heap_check_r;
SELECT COUNT(*) FROM dml_heap_check_r;

INSERT INTO dml_heap_check_r VALUES(DEFAULT,DEFAULT,'rn',0);
SELECT SUM(a),SUM(b) FROM dml_heap_check_r;
SELECT COUNT(*) FROM dml_heap_check_r;

INSERT INTO dml_heap_check_r VALUES(110,NULL,'rn',0);
SELECT SUM(a),SUM(b) FROM dml_heap_check_r;
SELECT COUNT(*) FROM dml_heap_check_r;

SELECT SUM(a) FROM dml_heap_check_r;
DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_heap_check_r ORDER BY 1;

UPDATE dml_heap_check_r set a = 110;
SELECT SUM(a) FROM dml_heap_check_r;
-- THIS SQL CONFIRMS THAT POST UPDATE THE OID OF THE TUPLE REMAINS THE SAME
SELECT * FROM ( (SELECT COUNT(*) FROM dml_heap_check_r) UNION (SELECT COUNT(*) FROM tempoid, dml_heap_check_r WHERE tempoid.oid = dml_heap_check_r.oid AND tempoid.gp_segment_id = dml_heap_check_r.gp_segment_id))foo;
