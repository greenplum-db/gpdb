\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_heap_check_r;
CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105), 
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text, 
	d numeric NOT NULL) 
WITH OIDS DISTRIBUTED BY (a);
