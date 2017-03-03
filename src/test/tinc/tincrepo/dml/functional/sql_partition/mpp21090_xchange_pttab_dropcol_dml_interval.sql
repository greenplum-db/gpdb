-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_interval;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int,
    col5 interval
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_interval VALUES('10 secs','10 secs','a',0, '10 secs');
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_interval DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_interval_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_interval_candidate( like mpp21090_xchange_pttab_dropcol_dml_interval) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_interval_candidate VALUES('10 secs','z',1,'10 secs');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_interval EXCHANGE PARTITION FOR('30 secs') WITH TABLE mpp21090_xchange_pttab_dropcol_dml_interval_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_interval SELECT  '1 sec','b', 1, '1 sec';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_interval SET col5 = '14 hours' WHERE col2 = '1 sec' AND col5 = '1 sec';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_interval SET col2 ='10 secs' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_interval WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_interval_candidate SELECT '1 sec','b', 1, '1 sec';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_interval_candidate SET col2='10 secs' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_interval_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_interval_candidate ORDER BY 1,2,3;
