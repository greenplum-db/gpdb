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
DROP TABLE IF EXISTS colalias_dml_char;
CREATE TABLE colalias_dml_char
(
    col1 char DEFAULT 'a',
    col2 char,
    col3 char,
    col4 char,
    col5 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_char_candidate;
CREATE TABLE colalias_dml_char_candidate
(
    col1 char DEFAULT 'a',
    col2 char,
    col3 char,
    col4 char,
    col5 char
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_char_candidate VALUES('g','a','a','g','a');

INSERT INTO colalias_dml_char(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_char_candidate)foo;
SELECT * FROM colalias_dml_char ORDER BY 1,2,3,4;

UPDATE colalias_dml_char SET col1 = (select col2 as col1 FROM colalias_dml_char_candidate);
SELECT * FROM colalias_dml_char ORDER BY 1,2,3,4;

UPDATE colalias_dml_char SET col1 =colalias_dml_char_candidate.col2 FROM colalias_dml_char_candidate;
SELECT * FROM colalias_dml_char ORDER BY 1,2,3,4;


