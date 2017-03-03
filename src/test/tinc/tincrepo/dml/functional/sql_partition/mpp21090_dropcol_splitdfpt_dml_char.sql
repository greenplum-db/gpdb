-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitdfpt_dml_char;
CREATE TABLE mpp21090_dropcol_splitdfpt_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_splitdfpt_dml_char VALUES('x','x','a','x',0);
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitdfpt_dml_char DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitdfpt_dml_char SPLIT DEFAULT PARTITION at ('d') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_splitdfpt_dml_char SELECT 'a', 'a','b', 1;
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitdfpt_dml_char SET col1 = 'z' WHERE col2 = 'a' AND col1 = 'a';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitdfpt_dml_char SET col2 = 'z' WHERE col2 = 'a' AND col1 = 'z';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitdfpt_dml_char WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_char ORDER BY 1,2,3;
