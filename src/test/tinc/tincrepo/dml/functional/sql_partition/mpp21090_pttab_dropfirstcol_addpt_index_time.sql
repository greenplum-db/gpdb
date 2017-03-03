-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_time;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_time VALUES('12:00:00','12:00:00','a','12:00:00',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_time;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_time on mpp21090_pttab_dropfirstcol_addpt_index_time(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_time DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_time SELECT '11:30:00','b','11:30:00', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_time SET col4 = '1:00:00' WHERE col2 = '11:30:00' AND col4 = '11:30:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col4 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

