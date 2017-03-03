-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_int;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int VALUES(20000,20000,'a',0, 20000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_int DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_int_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_int_candidate( like mpp21090_xchange_pttab_dropcol_dml_int) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int_candidate VALUES(5000,'z',1,5000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_int EXCHANGE PARTITION FOR(5000) WITH TABLE mpp21090_xchange_pttab_dropcol_dml_int_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int SELECT  1000,'b', 1, 1000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_int SET col5 = 35000 WHERE col2 = 1000 AND col5 = 1000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_int SET col2 =20000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_int WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int_candidate SELECT 1000,'b', 1, 1000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_int_candidate SET col2=2000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_int_candidate WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int_candidate ORDER BY 1,2,3;
