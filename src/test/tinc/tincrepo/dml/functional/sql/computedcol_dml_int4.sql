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
DROP TABLE IF EXISTS computedcol_dml_int4;
CREATE TABLE computedcol_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001), default partition def);

INSERT INTO computedcol_dml_int4(col2,col1,col3,col5,col4) SELECT 20000000::int4+10000000::int4, 20000000::int4+10000000::int4, 'a', 1,20000000::int4+10000000::int4;
INSERT INTO computedcol_dml_int4(col2,col3,col5,col4) SELECT 20000000::int4+10000000::int4,'a', 1,20000000::int4+10000000::int4; 
SELECT * FROM computedcol_dml_int4 ORDER BY 1,2,3,4;

UPDATE computedcol_dml_int4 SET col1=20000000::int4+10000000::int4;
SELECT * FROM computedcol_dml_int4 ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_int4 WHERE col1=10000000::int4 + 20000000::int4;
SELECT * FROM computedcol_dml_int4 ORDER BY 1,2,3,4;

