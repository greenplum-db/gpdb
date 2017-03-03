-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS computedcol_dml_int8;
CREATE TABLE computedcol_dml_int8
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001), default partition def);

INSERT INTO computedcol_dml_int8(col2,col1,col3,col5,col4) SELECT 200000000000000000::int8+1000000000000000000::int8, 200000000000000000::int8+1000000000000000000::int8, 'a', 1,200000000000000000::int8+1000000000000000000::int8;
INSERT INTO computedcol_dml_int8(col2,col3,col5,col4) SELECT 200000000000000000::int8+1000000000000000000::int8,'a', 1,200000000000000000::int8+1000000000000000000::int8; 
SELECT * FROM computedcol_dml_int8 ORDER BY 1,2,3,4;

UPDATE computedcol_dml_int8 SET col1=200000000000000000::int8+1000000000000000000::int8;
SELECT * FROM computedcol_dml_int8 ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_int8 WHERE col1=1000000000000000000::int8 + 200000000000000000::int8;
SELECT * FROM computedcol_dml_int8 ORDER BY 1,2,3,4;

