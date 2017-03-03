-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_date;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_date VALUES('2013-12-30','2013-12-30','a','2013-12-30',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_date DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_date VALUES('2013-12-30','2013-12-30','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_date SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_date SELECT '2014-01-01', '2014-01-01','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_date SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_date WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

