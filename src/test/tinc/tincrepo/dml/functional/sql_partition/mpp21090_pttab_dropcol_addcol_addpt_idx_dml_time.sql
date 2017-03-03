-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time VALUES('12:00:00','12:00:00','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ADD COLUMN col5 time DEFAULT '12:00:00';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_time;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_time on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SELECT '12:00:00','12:00:00','c','12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SET col5 = '1:00:00' WHERE col2 = '12:00:00' AND col1 = '12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time WHERE col5 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ADD PARTITION partfour start('11:00:00') end('12:00:00');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SELECT '11:30:00','11:30:00','d','11:30:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SET col5 = '1:00:00' WHERE col2 = '11:30:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time WHERE col5 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

