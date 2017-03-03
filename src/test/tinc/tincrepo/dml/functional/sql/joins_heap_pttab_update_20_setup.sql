\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_heap_pt_u;
DROP TABLE IF EXISTS dml_heap_pt_v;
CREATE TABLE dml_heap_pt_u as SELECT i as a, i as b  FROM generate_series(1,10)i;
CREATE TABLE dml_heap_pt_v as SELECT i as a, i as b FROM generate_series(1,10)i;
