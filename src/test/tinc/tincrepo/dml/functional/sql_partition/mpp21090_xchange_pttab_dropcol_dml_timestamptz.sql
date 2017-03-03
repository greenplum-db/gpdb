-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_timestamptz;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int,
    col5 timestamptz
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a',0, '2013-12-30 12:00:00 PST');
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_timestamptz DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate( like mpp21090_xchange_pttab_dropcol_dml_timestamptz) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate VALUES('2013-12-30 12:00:00 PST','z',1,'2013-12-30 12:00:00 PST');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_timestamptz EXCHANGE PARTITION FOR('2013-12-15 12:00:00 PST') WITH TABLE mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamptz SELECT  '2013-12-01 12:00:00 PST','b', 1, '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_timestamptz SET col5 = '2014-02-10 12:00:00 PST' WHERE col2 = '2013-12-01 12:00:00 PST' AND col5 = '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_timestamptz SET col2 ='2013-12-30 12:00:00 PST' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate SELECT '2013-12-01 12:00:00 PST','b', 1, '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate SET col2='2013-12-30 12:00:00 PST' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_timestamptz_candidate ORDER BY 1,2,3;
