-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_int4;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 VALUES(20000000,20000000,'a',20000000);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4 DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 VALUES(20000000,20000000,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4 ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 VALUES(20000000,20000000,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4 SPLIT DEFAULT PARTITION at (50000000) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 SELECT 10000000, 10000000,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int4 SET col1 = 35000000 WHERE col2 = 10000000 AND col1 = 10000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int4 SET col2 = 35000000 WHERE col2 = 10000000 AND col1 = 35000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;
