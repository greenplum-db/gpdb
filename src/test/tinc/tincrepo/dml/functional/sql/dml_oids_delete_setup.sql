\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_heap_r;
CREATE TABLE dml_heap_r (a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
INSERT INTO dml_heap_r VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,NULL);
SELECT COUNT(*) FROM dml_heap_r;
