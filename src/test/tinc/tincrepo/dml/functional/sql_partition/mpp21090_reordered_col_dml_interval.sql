-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_interval;
CREATE TABLE mpp21090_reordered_col_dml_interval
(
    col1 interval DEFAULT '11 hours',
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_reordered_col_dml_interval(col2,col1,col3,col5,col4) SELECT '10 secs', '10 secs','a', 1,'10 secs';
INSERT INTO mpp21090_reordered_col_dml_interval(col2,col3,col5,col4) SELECT '10 secs','b', 2, '10 secs'; 
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_interval DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_interval ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_interval(col2,col3,col5,col4) SELECT '10 secs','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_interval SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_interval WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

