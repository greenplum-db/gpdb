-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_int8;
CREATE TABLE mpp21090_reordered_col_dml_int8
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_reordered_col_dml_int8(col2,col1,col3,col5,col4) SELECT 200000000000000000, 200000000000000000,'a', 1,200000000000000000;
INSERT INTO mpp21090_reordered_col_dml_int8(col2,col3,col5,col4) SELECT 200000000000000000,'b', 2, 200000000000000000; 
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_int8 DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_int8 ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_int8(col2,col3,col5,col4) SELECT 200000000000000000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_int8 SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_int8 WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

