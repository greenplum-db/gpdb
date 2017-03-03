-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_timestamptz;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz ADD COLUMN col5 timestamptz DEFAULT '2013-12-31 12:00:00 PST';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b',1, '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamptz SELECT '2014-02-10 12:00:00 PST','c',1, '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_timestamptz SET col5 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

