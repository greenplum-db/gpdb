-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_numeric;
CREATE TABLE mpp21090_reordered_col_dml_numeric
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_reordered_col_dml_numeric(col2,col1,col3,col5,col4) SELECT 2.000000, 2.000000,'a', 1,2.000000;
INSERT INTO mpp21090_reordered_col_dml_numeric(col2,col3,col5,col4) SELECT 2.000000,'b', 2, 2.000000; 
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_numeric DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_numeric ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_numeric(col2,col3,col5,col4) SELECT 2.000000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_numeric SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_numeric WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

