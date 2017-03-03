-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_firstcol_dml_decimal;
CREATE TABLE mpp21090_drop_firstcol_dml_decimal(
col1 decimal,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by(col3);
INSERT INTO mpp21090_drop_firstcol_dml_decimal VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_firstcol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_firstcol_dml_decimal DROP COLUMN col1;
INSERT INTO mpp21090_drop_firstcol_dml_decimal SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_firstcol_dml_decimal ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_firstcol_dml_decimal SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_firstcol_dml_decimal ORDER BY 1,2,3,4;
DELETE FROM mpp21090_drop_firstcol_dml_decimal WHERE col3='c';
SELECT * FROM mpp21090_drop_firstcol_dml_decimal ORDER BY 1,2,3,4;

