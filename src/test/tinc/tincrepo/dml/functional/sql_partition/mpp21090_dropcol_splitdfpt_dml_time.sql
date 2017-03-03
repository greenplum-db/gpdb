-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitdfpt_dml_time;
CREATE TABLE mpp21090_dropcol_splitdfpt_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_splitdfpt_dml_time VALUES('12:00:00','12:00:00','a','12:00:00',0);
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitdfpt_dml_time DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitdfpt_dml_time SPLIT DEFAULT PARTITION at ('15:00:00') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_splitdfpt_dml_time SELECT '12:00:00', '12:00:00','b', 1;
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitdfpt_dml_time SET col1 = '11:30:00' WHERE col2 = '12:00:00' AND col1 = '12:00:00';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitdfpt_dml_time SET col2 = '11:30:00' WHERE col2 = '12:00:00' AND col1 = '11:30:00';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitdfpt_dml_time WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitdfpt_dml_time ORDER BY 1,2,3;
