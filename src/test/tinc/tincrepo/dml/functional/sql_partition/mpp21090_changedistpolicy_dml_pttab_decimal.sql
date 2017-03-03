-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_decimal;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_decimal VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_decimal DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_decimal VALUES(2.00,2.00,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_decimal SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_decimal SELECT 1.00, 1.00,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_decimal SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_decimal WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

