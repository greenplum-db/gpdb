-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_timestamptz;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int,
    col5 timestamptz
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0,'2013-12-31 12:00:00 PST');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamptz DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamptz SET col1 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

