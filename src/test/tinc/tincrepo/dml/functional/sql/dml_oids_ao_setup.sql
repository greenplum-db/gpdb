\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_ao;
CREATE TABLE dml_ao (a int , b int default -1, c text) WITH (appendonly = true, oids = true) DISTRIBUTED BY (a);
INSERT INTO dml_ao VALUES(generate_series(1,2),generate_series(1,2),'r');
SELECT SUM(a),SUM(b) FROM dml_ao;
SELECT COUNT(*) FROM dml_ao;

INSERT INTO dml_ao VALUES(NULL,NULL,NULL);
SELECT SUM(a),SUM(b) FROM dml_ao;
SELECT COUNT(*) FROM dml_ao;

