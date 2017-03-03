-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_timestamptz;
CREATE TABLE mpp21090_reordered_col_dml_timestamptz
(
    col1 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_reordered_col_dml_timestamptz(col2,col1,col3,col5,col4) SELECT '2013-12-30 12:00:00 PST', '2013-12-30 12:00:00 PST','a', 1,'2013-12-30 12:00:00 PST';
INSERT INTO mpp21090_reordered_col_dml_timestamptz(col2,col3,col5,col4) SELECT '2013-12-30 12:00:00 PST','b', 2, '2013-12-30 12:00:00 PST'; 
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_timestamptz DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_timestamptz ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_timestamptz(col2,col3,col5,col4) SELECT '2013-12-30 12:00:00 PST','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_timestamptz SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_timestamptz WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

