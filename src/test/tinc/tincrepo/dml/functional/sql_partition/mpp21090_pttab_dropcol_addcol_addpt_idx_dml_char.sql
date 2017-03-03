-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char VALUES('g','g','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char VALUES('g','g','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ADD COLUMN col5 char DEFAULT 'g';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_char;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_char on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SELECT 'g','g','c','g';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SET col5 = 'a' WHERE col2 = 'g' AND col1 = 'g';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char WHERE col5 = 'a';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ADD PARTITION partfour VALUES('y','z','-');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SELECT 'z','z','d','z';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SET col5 = 'a' WHERE col2 = 'z' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SET col2 = 'a' WHERE col2 = 'z' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char WHERE col5 = 'a';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

