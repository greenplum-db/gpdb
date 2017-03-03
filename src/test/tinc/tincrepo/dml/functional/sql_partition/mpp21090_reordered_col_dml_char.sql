-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_char;
CREATE TABLE mpp21090_reordered_col_dml_char
(
    col1 char DEFAULT 'a',
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_reordered_col_dml_char(col2,col1,col3,col5,col4) SELECT 'g', 'g','a', 1,'g';
INSERT INTO mpp21090_reordered_col_dml_char(col2,col3,col5,col4) SELECT 'g','b', 2, 'g'; 
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_char DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_char ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_char(col2,col3,col5,col4) SELECT 'g','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_char SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_char WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

