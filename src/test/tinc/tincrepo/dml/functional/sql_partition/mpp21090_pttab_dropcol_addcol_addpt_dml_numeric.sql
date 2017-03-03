-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_numeric;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric VALUES(2.000000,2.000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ADD COLUMN col5 numeric DEFAULT 2.000000;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SELECT 2.000000,2.000000,'c',2.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SET col5 = 1.000000 WHERE col2 = 2.000000 AND col1 = 2.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric WHERE col5 = 1.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SELECT 35.000000,35.000000,'d',35.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SET col5 = 1.000000 WHERE col2 = 35.000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric WHERE col5 = 1.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

