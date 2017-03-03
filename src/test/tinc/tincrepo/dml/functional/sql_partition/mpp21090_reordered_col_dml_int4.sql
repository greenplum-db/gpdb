-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_int4;
CREATE TABLE mpp21090_reordered_col_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_reordered_col_dml_int4(col2,col1,col3,col5,col4) SELECT 20000000, 20000000,'a', 1,20000000;
INSERT INTO mpp21090_reordered_col_dml_int4(col2,col3,col5,col4) SELECT 20000000,'b', 2, 20000000; 
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_int4 DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_int4 ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_int4(col2,col3,col5,col4) SELECT 20000000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_int4 SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_int4 WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

