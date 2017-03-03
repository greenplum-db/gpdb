-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_char;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_char VALUES('g','g','a','g',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_char DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_char VALUES('g','g','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_char SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_char SELECT 'a', 'a','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_char SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_char WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

