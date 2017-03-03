-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_int;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_int VALUES(10000,10000,'a',10000,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int VALUES(10000,10000,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int SELECT 20000, 20000,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_int SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_int WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

