-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_date;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_date VALUES('2013-12-31','2013-12-31','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_date DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_date SELECT '2014-02-10','2014-02-10','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_date ADD COLUMN col5 date;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_date SELECT '2013-12-31','2013-12-31','c','2013-12-31';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_date SET col1 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-02-10';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-01-01';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

