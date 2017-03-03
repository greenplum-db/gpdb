-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_lastcol_index_dml_int4;
CREATE TABLE mpp21090_drop_lastcol_index_dml_int4(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 int4
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col3);

DROP INDEX IF EXISTS mpp21090_drop_lastcol_index_dml_idx_int4;
CREATE INDEX mpp21090_drop_lastcol_index_dml_idx_int4 on mpp21090_drop_lastcol_index_dml_int4(col3);

INSERT INTO mpp21090_drop_lastcol_index_dml_int4 VALUES(0,0.00,'a','2014-01-01',20000000);
SELECT * FROM mpp21090_drop_lastcol_index_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_drop_lastcol_index_dml_int4 DROP COLUMN col5;

INSERT INTO mpp21090_drop_lastcol_index_dml_int4 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_lastcol_index_dml_int4 ORDER BY 1,2,3,4;

UPDATE mpp21090_drop_lastcol_index_dml_int4 SET col3='c' WHERE col3 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_lastcol_index_dml_int4 ORDER BY 1,2,3,4;

DELETE FROM mpp21090_drop_lastcol_index_dml_int4 WHERE col3='c';
SELECT * FROM mpp21090_drop_lastcol_index_dml_int4 ORDER BY 1,2,3,4;

