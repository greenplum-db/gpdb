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
DROP TABLE IF EXISTS colalias_dml_int4;
CREATE TABLE colalias_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int4
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_int4_candidate;
CREATE TABLE colalias_dml_int4_candidate
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int4
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_int4_candidate VALUES(20000000,10000000,'a',20000000,10000000);

INSERT INTO colalias_dml_int4(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_int4_candidate)foo;
SELECT * FROM colalias_dml_int4 ORDER BY 1,2,3,4;

UPDATE colalias_dml_int4 SET col1 = (select col2 as col1 FROM colalias_dml_int4_candidate);
SELECT * FROM colalias_dml_int4 ORDER BY 1,2,3,4;

UPDATE colalias_dml_int4 SET col1 =colalias_dml_int4_candidate.col2 FROM colalias_dml_int4_candidate;
SELECT * FROM colalias_dml_int4 ORDER BY 1,2,3,4;


