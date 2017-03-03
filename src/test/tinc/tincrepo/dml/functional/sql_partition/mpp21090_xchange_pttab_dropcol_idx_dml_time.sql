-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_idx_dml_time;
CREATE TABLE mpp21090_xchange_pttab_dropcol_idx_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int,
    col5 time
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_time VALUES('12:00:00','12:00:00','a',0, '12:00:00');
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time ORDER BY 1,2,3,4;

DROP INDEX IF EXISTS mpp21090_xchange_pttab_dropcol_idx_dml_idx_time;
CREATE INDEX mpp21090_xchange_pttab_dropcol_idx_dml_idx_time on mpp21090_xchange_pttab_dropcol_idx_dml_time(col2);

ALTER TABLE mpp21090_xchange_pttab_dropcol_idx_dml_time DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate( like mpp21090_xchange_pttab_dropcol_idx_dml_time) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate VALUES('12:00:00','z',1,'12:00:00');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_idx_dml_time EXCHANGE PARTITION FOR('15:00:00') WITH TABLE mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_time SELECT  '12:00:00','b', 1, '12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_idx_dml_time SET col5 = '11:30:00' WHERE col2 = '12:00:00' AND col5 = '12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_idx_dml_time SET col2 ='12:00:00' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_idx_dml_time WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate SELECT '12:00:00','b', 1, '12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate SET col2='12:00:00' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_idx_dml_time_candidate ORDER BY 1,2,3;
