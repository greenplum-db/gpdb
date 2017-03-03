-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_decimal;
CREATE TABLE mpp21090_reordered_col_dml_decimal
(
    col1 decimal DEFAULT 1.00,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_reordered_col_dml_decimal(col2,col1,col3,col5,col4) SELECT 2.00, 2.00,'a', 1,2.00;
INSERT INTO mpp21090_reordered_col_dml_decimal(col2,col3,col5,col4) SELECT 2.00,'b', 2, 2.00; 
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_decimal DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_decimal ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_decimal(col2,col3,col5,col4) SELECT 2.00,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_decimal SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_decimal WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

