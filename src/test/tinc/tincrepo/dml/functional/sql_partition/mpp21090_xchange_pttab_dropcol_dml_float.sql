-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_float;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int,
    col5 float
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_float VALUES(2.00,2.00,'a',0, 2.00);
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_float DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_float_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_float_candidate( like mpp21090_xchange_pttab_dropcol_dml_float) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_float_candidate VALUES(2.00,'z',1,2.00);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_float EXCHANGE PARTITION FOR(5.00) WITH TABLE mpp21090_xchange_pttab_dropcol_dml_float_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_float SELECT  1.00,'b', 1, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_float SET col5 = 35.00 WHERE col2 = 1.00 AND col5 = 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_float SET col2 =2.00 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_float WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_float_candidate SELECT 1.00,'b', 1, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_float_candidate SET col2=2.00 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_float_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_float_candidate ORDER BY 1,2,3;
