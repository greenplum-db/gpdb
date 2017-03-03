-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_char;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_char VALUES('x','x','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_char SELECT 'z','z','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_char DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_char SELECT 'z','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_char SET col4 = 10 WHERE col2 = 'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_char SET col2 = '-' WHERE col2 = 'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

