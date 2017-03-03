-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dropped_col_tab;
CREATE TABLE dropped_col_tab(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 int
) distributed by (col1);
INSERT INTO dropped_col_tab VALUES(0,0.00,'a','2014-01-01',0);
SELECT * FROM dropped_col_tab ORDER BY 1,2,3;

ALTER TABLE dropped_col_tab DROP COLUMN col1;
ALTER TABLE dropped_col_tab DROP COLUMN col2;
ALTER TABLE dropped_col_tab DROP COLUMN col3;
ALTER TABLE dropped_col_tab DROP COLUMN col4;
ALTER TABLE dropped_col_tab DROP COLUMN col5;

INSERT INTO dropped_col_tab SELECT 1;
SELECT * FROM dropped_col_tab;
