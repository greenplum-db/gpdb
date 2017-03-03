-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_int8;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ADD COLUMN col5 int8 DEFAULT 2000000000000000000;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SELECT 2000000000000000000,2000000000000000000,'c',2000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SET col5 = 1000000000000000000 WHERE col2 = 2000000000000000000 AND col1 = 2000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 WHERE col5 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SELECT 3500000000000000000,3500000000000000000,'d',3500000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SET col5 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 WHERE col5 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

