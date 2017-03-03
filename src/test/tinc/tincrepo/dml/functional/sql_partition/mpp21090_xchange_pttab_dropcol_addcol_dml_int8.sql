-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int8;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int,
    col5 int8
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8 VALUES(200000000000000000,200000000000000000,'a',0, 200000000000000000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ADD COLUMN col1 int8 DEFAULT 200000000000000000;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_int8) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate VALUES(200000000000000000,'z',1,200000000000000000,200000000000000000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 EXCHANGE PARTITION FOR(500000000000000000) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8 SELECT  100000000000000000,'b', 1, 100000000000000000, 100000000000000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 SET col5 = 3500000000000000000 WHERE col2 = 100000000000000000 AND col5 = 100000000000000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 SET col2 =200000000000000000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate SELECT 100000000000000000,'b', 1, 100000000000000000, 100000000000000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate SET col2=200000000000000000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;
