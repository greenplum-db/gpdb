-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_decimal;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal ADD COLUMN col5 decimal DEFAULT 2.00;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_decimal SELECT 35.00,35.00,'b',1, 35.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_decimal SELECT 35.00,'c',1, 35.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_decimal SET col5 = 1.00 WHERE col2 = 35.00 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_decimal SET col2 = 1.00 WHERE col2 = 35.00 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

