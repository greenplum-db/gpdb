-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamptz;
CREATE TABLE mpp21090_drop_multicol_dml_timestamptz(
col1 timestamptz,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamptz SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamptz SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamptz WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;

