-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_int;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int VALUES(20000,20000,'a',20000,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int SELECT 35000,'b',35000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_int SET col4 = 10000 WHERE col2 = 35000 AND col4 = 35000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_int SET col2 = 10000 WHERE col2 = 35000 AND col4 = 10000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

