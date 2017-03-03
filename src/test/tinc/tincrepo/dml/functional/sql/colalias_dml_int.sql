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
DROP TABLE IF EXISTS colalias_dml_int;
CREATE TABLE colalias_dml_int
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_int_candidate;
CREATE TABLE colalias_dml_int_candidate
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_int_candidate VALUES(10000,20000,'a',10000,20000);

INSERT INTO colalias_dml_int(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_int_candidate)foo;
SELECT * FROM colalias_dml_int ORDER BY 1,2,3,4;

UPDATE colalias_dml_int SET col1 = (select col2 as col1 FROM colalias_dml_int_candidate);
SELECT * FROM colalias_dml_int ORDER BY 1,2,3,4;

UPDATE colalias_dml_int SET col1 =colalias_dml_int_candidate.col2 FROM colalias_dml_int_candidate;
SELECT * FROM colalias_dml_int ORDER BY 1,2,3,4;


