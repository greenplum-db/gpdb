\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) WITH OIDS DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (col1 serial, a numeric, b decimal) WITH OIDS DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_p(a,b) SELECT id as a, id as b FROM (SELECT * FROM generate_series(1,2) as id) AS x;
INSERT INTO dml_heap_p(a,b) VALUES(NULL,NULL);

INSERT INTO dml_heap_r VALUES(generate_series(1,2),generate_series(1,2),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,NULL);

SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM dml_heap_p;

