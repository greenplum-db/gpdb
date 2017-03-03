-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_interval;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_interval VALUES('10 secs','10 secs','a','10 secs',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_interval DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_interval VALUES('10 secs','10 secs','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_interval SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_interval SELECT '11 hours', '11 hours','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_interval SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_interval WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

