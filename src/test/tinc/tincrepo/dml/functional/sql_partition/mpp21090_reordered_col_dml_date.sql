-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_date;
CREATE TABLE mpp21090_reordered_col_dml_date
(
    col1 date DEFAULT '2014-01-01',
    col2 date,
    col3 char,
    col4 date,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_reordered_col_dml_date(col2,col1,col3,col5,col4) SELECT '2013-12-30', '2013-12-30','a', 1,'2013-12-30';
INSERT INTO mpp21090_reordered_col_dml_date(col2,col3,col5,col4) SELECT '2013-12-30','b', 2, '2013-12-30'; 
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_date DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_date ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_date(col2,col3,col5,col4) SELECT '2013-12-30','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_date SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_date WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

