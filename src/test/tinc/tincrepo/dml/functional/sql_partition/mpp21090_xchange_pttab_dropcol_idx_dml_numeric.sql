-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_idx_dml_numeric;
CREATE TABLE mpp21090_xchange_pttab_dropcol_idx_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int,
    col5 numeric
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_numeric VALUES(2.000000,2.000000,'a',0, 2.000000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric ORDER BY 1,2,3,4;

DROP INDEX IF EXISTS mpp21090_xchange_pttab_dropcol_idx_dml_idx_numeric;
CREATE INDEX mpp21090_xchange_pttab_dropcol_idx_dml_idx_numeric on mpp21090_xchange_pttab_dropcol_idx_dml_numeric(col2);

ALTER TABLE mpp21090_xchange_pttab_dropcol_idx_dml_numeric DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate( like mpp21090_xchange_pttab_dropcol_idx_dml_numeric) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate VALUES(2.000000,'z',1,2.000000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_idx_dml_numeric EXCHANGE PARTITION FOR(5.000000) WITH TABLE mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_numeric SELECT  1.000000,'b', 1, 1.000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_idx_dml_numeric SET col5 = 35.000000 WHERE col2 = 1.000000 AND col5 = 1.000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_idx_dml_numeric SET col2 =2.000000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate SELECT 1.000000,'b', 1, 1.000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate SET col2=2.000000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_numeric_candidate ORDER BY 1,2,3;
