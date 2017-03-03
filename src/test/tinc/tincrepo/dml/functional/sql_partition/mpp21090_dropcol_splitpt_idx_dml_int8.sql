-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_int8;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_int8;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_int8 on mpp21090_dropcol_splitpt_idx_dml_int8(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int8 DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int8 SPLIT PARTITION partone at (500000000000000000) into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int8 SELECT 100000000000000000, 100000000000000000,'b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int8 SET col1 = 3500000000000000000 WHERE col2 = 100000000000000000 AND col1 = 100000000000000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int8 SET col2 =200000000000000000  WHERE col2 = 100000000000000000 AND col1 = 3500000000000000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_int8 WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;
