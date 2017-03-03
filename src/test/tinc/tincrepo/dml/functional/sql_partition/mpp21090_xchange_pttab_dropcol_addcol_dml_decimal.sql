-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_decimal;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int,
    col5 decimal
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal VALUES(2.00,2.00,'a',0, 2.00);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ADD COLUMN col1 decimal DEFAULT 2.00;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_decimal) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate VALUES(2.00,'z',1,2.00,2.00);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal EXCHANGE PARTITION FOR(5.00) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal SELECT  1.00,'b', 1, 1.00, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal SET col5 = 35.00 WHERE col2 = 1.00 AND col5 = 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal SET col2 =2.00 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate SELECT 1.00,'b', 1, 1.00, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate SET col2=2.00 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;
