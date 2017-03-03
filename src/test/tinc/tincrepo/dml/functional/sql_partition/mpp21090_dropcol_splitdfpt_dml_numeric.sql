-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitdfpt_dml_numeric;
CREATE TABLE mpp21090_dropcol_splitdfpt_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_splitdfpt_dml_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitdfpt_dml_numeric DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitdfpt_dml_numeric SPLIT DEFAULT PARTITION at (5.000000) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_splitdfpt_dml_numeric SELECT 1.000000, 1.000000,'b', 1;
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitdfpt_dml_numeric SET col1 = 35.000000 WHERE col2 = 1.000000 AND col1 = 1.000000;
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitdfpt_dml_numeric SET col2 = 35.000000 WHERE col2 = 1.000000 AND col1 = 35.000000;
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitdfpt_dml_numeric WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_numeric ORDER BY 1,2,3;
