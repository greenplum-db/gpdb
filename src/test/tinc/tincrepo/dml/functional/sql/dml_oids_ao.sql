-- @author prabhd
-- @created 2013-07-08 12:00:00
-- @modified 2013-07-08 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description DDL on AO/CO tables with OIDS(Negative Test)
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,a FROM dml_ao ORDER BY 1;
UPDATE dml_ao SET a = 100;
SELECT * FROM ( (SELECT COUNT(*) FROM dml_ao) UNION (SELECT COUNT(*) FROM tempoid, dml_ao WHERE tempoid.oid = dml_ao.oid AND tempoid.gp_segment_id = dml_ao.gp_segment_id))foo;
