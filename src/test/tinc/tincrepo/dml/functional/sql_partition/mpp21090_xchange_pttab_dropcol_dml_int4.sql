-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_int4;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int,
    col5 int4
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int4 VALUES(20000000,20000000,'a',0, 20000000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_int4 DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_int4_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_int4_candidate( like mpp21090_xchange_pttab_dropcol_dml_int4) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int4_candidate VALUES(20000000,'z',1,20000000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_int4 EXCHANGE PARTITION FOR(50000000) WITH TABLE mpp21090_xchange_pttab_dropcol_dml_int4_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4 ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int4 SELECT  10000000,'b', 1, 10000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_int4 SET col5 = 350000000 WHERE col2 = 10000000 AND col5 = 10000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_int4 SET col2 =20000000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_int4 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4 ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_int4_candidate SELECT 10000000,'b', 1, 10000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_int4_candidate SET col2=20000000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_int4_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_int4_candidate ORDER BY 1,2,3;
