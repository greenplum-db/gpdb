-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS changedistpolicy_dml_pttab_numeric;
CREATE TABLE changedistpolicy_dml_pttab_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO changedistpolicy_dml_pttab_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_numeric SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_numeric SELECT 1.000000, 1.000000,'b', 1.000000, 1;
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_numeric SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_numeric WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

