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
DROP TABLE IF EXISTS computedcol_dml_int;
CREATE TABLE computedcol_dml_int
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001), default partition def);

INSERT INTO computedcol_dml_int(col2,col1,col3,col5,col4) SELECT 10000::int+20000::int, 10000::int+20000::int, 'a', 1,10000::int+20000::int;
INSERT INTO computedcol_dml_int(col2,col3,col5,col4) SELECT 10000::int+20000::int,'a', 1,10000::int+20000::int; 
SELECT * FROM computedcol_dml_int ORDER BY 1,2,3,4;

UPDATE computedcol_dml_int SET col1=10000::int+20000::int;
SELECT * FROM computedcol_dml_int ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_int WHERE col1=20000::int + 10000::int;
SELECT * FROM computedcol_dml_int ORDER BY 1,2,3,4;

