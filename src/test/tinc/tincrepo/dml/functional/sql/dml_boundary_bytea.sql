-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bytea
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_bytea;
CREATE TABLE dml_bytea( a bytea) distributed by (a);

-- Simple DML
INSERT INTO dml_bytea VALUES(pg_catalog.decode(array_to_string(ARRAY(SELECT chr((65 + round(random() * 25)) :: integer) FROM generate_series(1, 10490000)), ''),'escape'));

SELECT COUNT(*) FROM dml_bytea;

UPDATE dml_bytea SET a = pg_catalog.decode(array_to_string(ARRAY(SELECT chr((65 + round(random() * 25)) :: integer) FROM generate_series(1, 10490000)), ''),'escape'); 


