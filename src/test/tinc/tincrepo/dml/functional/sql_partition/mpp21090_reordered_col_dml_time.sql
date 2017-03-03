-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_time;
CREATE TABLE mpp21090_reordered_col_dml_time
(
    col1 time DEFAULT '1:00:00',
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_reordered_col_dml_time(col2,col1,col3,col5,col4) SELECT '12:00:00', '12:00:00','a', 1,'12:00:00';
INSERT INTO mpp21090_reordered_col_dml_time(col2,col3,col5,col4) SELECT '12:00:00','b', 2, '12:00:00'; 
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_time DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_time ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_time(col2,col3,col5,col4) SELECT '12:00:00','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_time SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_time WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

