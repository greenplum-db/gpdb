-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_date;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int,
    col5 date
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date VALUES('2013-12-30','2013-12-30','a',0, '2013-12-30');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date ADD COLUMN col1 date DEFAULT '2013-12-30';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_date) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate VALUES('2013-12-30','z',1,'2013-12-30','2013-12-30');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date EXCHANGE PARTITION FOR('2013-12-15') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date SELECT  '2013-12-01','b', 1, '2013-12-01', '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_date SET col5 = '2014-02-10' WHERE col2 = '2013-12-01' AND col5 = '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_date SET col2 ='2013-12-30' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate SELECT '2013-12-01','b', 1, '2013-12-01', '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate SET col2='2013-12-30' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;
