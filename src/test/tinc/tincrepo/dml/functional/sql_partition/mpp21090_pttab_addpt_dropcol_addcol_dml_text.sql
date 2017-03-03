-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_text;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text SELECT 'xyz','xyz','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text SELECT 'xyz','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text ADD COLUMN col5 text DEFAULT 'abcdefghijklmnop';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text SELECT 'xyz','d',1,'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_text SET col4 = 10 WHERE col2 = 'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

