-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_text;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 text,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a','abcdefghijklmnop',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_text DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_dropmidcol_addpt_text SELECT 'xyz', 'xyz','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_text SET col1 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'xyz';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

