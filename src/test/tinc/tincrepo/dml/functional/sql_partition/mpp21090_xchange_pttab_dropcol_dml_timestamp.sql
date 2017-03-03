-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_timestamp;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int,
    col5 timestamp
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a',0, '2013-12-30 12:00:00');
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_timestamp DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate( like mpp21090_xchange_pttab_dropcol_dml_timestamp) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate VALUES('2013-12-30 12:00:00','z',1,'2013-12-30 12:00:00');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_timestamp EXCHANGE PARTITION FOR('2013-12-15 12:00:00') WITH TABLE mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamp SELECT  '2013-12-01 12:00:00','b', 1, '2013-12-01 12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_timestamp SET col5 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col5 = '2013-12-01 12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_timestamp SET col2 ='2013-12-30 12:00:00' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_timestamp WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate SELECT '2013-12-01 12:00:00','b', 1, '2013-12-01 12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate SET col2='2013-12-30 12:00:00' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamp_candidate ORDER BY 1,2,3;
