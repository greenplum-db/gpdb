-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int;
CREATE TABLE mpp21090_drop_midcol_dml_int
(
col1 int,
col2 decimal,
col3 int,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int VALUES(0,0.00,20000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
