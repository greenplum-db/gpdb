-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_char;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int,
    col5 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char VALUES('g','g','a',0, 'g');
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_char DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_char_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_char_candidate( like mpp21090_xchange_pttab_dropcol_dml_char) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char_candidate VALUES('g','z',1,'g');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_char EXCHANGE PARTITION FOR('d') WITH TABLE mpp21090_xchange_pttab_dropcol_dml_char_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char SELECT  'a','b', 1, 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_char SET col5 = 'z' WHERE col2 = 'a' AND col5 = 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_char SET col2 ='g' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_char WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char_candidate SELECT 'a','b', 1, 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_char_candidate SET col2='g' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;
