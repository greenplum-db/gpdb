-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitpt_dml_int;
CREATE TABLE mpp21090_dropcol_addcol_splitpt_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_int VALUES(20000,20000,'a',20000);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_int ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_int VALUES(20000,20000,'b',20000,0);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_int DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_int VALUES(20000,20000,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_int SPLIT PARTITION partone at (5000) into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_int SELECT 1000, 1000,'d', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitpt_dml_int SET col1 = 35000 WHERE col2 = 1000 AND col1 = 1000;
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitpt_dml_int SET col2 =20000  WHERE col2 = 1000 AND col1 = 35000;
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitpt_dml_int WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_int ORDER BY 1,2,3;
