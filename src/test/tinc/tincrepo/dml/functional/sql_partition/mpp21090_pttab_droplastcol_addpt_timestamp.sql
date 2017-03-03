-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_timestamp;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int,
    col5 timestamp
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0,'2013-12-31 12:00:00');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamp DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamp SET col1 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

