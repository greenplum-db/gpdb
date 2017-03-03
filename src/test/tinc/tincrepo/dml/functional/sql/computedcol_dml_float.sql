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
DROP TABLE IF EXISTS computedcol_dml_float;
CREATE TABLE computedcol_dml_float
(
    col1 float DEFAULT 1.00,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00), default partition def);

INSERT INTO computedcol_dml_float(col2,col1,col3,col5,col4) SELECT 2.00::float+1.00::float, 2.00::float+1.00::float, 'a', 1,2.00::float+1.00::float;
INSERT INTO computedcol_dml_float(col2,col3,col5,col4) SELECT 2.00::float+1.00::float,'a', 1,2.00::float+1.00::float; 
SELECT * FROM computedcol_dml_float ORDER BY 1,2,3,4;

UPDATE computedcol_dml_float SET col1=2.00::float+1.00::float;
SELECT * FROM computedcol_dml_float ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_float WHERE col1=1.00::float + 2.00::float;
SELECT * FROM computedcol_dml_float ORDER BY 1,2,3,4;

