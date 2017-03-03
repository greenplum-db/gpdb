-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_timestamp;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a','2013-12-31 12:00:00');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SPLIT DEFAULT PARTITION at ('2013-12-15 12:00:00') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SELECT '2013-12-01 12:00:00', '2013-12-01 12:00:00','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SET col1 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col1 = '2013-12-01 12:00:00';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SET col2 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;
