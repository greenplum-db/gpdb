-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_char;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_char VALUES('g','g','a','g',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_char;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_char on mpp21090_pttab_dropfirstcol_addpt_index_char(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_char DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_char SELECT 'z','b','z', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_char SET col4 = 'a' WHERE col2 = 'z' AND col4 = 'z';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_char SET col2 = 'a' WHERE col2 = 'z' AND col4 = 'a';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_char WHERE col2 = 'a';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

