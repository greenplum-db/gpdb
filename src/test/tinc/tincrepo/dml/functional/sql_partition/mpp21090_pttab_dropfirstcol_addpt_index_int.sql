-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_int;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int VALUES(10000,10000,'a',10000,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_int;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_int on mpp21090_pttab_dropfirstcol_addpt_index_int(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int SELECT 35000,'b',35000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int SET col4 = 20000 WHERE col2 = 35000 AND col4 = 35000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int SET col2 = 20000 WHERE col2 = 35000 AND col4 = 20000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_int WHERE col2 = 20000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

