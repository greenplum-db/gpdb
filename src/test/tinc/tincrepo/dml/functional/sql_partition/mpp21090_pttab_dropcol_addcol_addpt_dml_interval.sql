-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_interval;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval VALUES('1 hour','1 hour','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval VALUES('1 hour','1 hour','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval ADD COLUMN col5 interval DEFAULT '1 hour';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval SELECT '1 hour','1 hour','c','1 hour';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_interval SET col5 = '12 hours' WHERE col2 = '1 hour' AND col1 = '1 hour';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval WHERE col5 = '12 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval SELECT '14 hours','14 hours','d','14 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_interval SET col5 = '12 hours' WHERE col2 = '14 hours' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_interval SET col2 = '12 hours' WHERE col2 = '14 hours' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval WHERE col5 = '12 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

