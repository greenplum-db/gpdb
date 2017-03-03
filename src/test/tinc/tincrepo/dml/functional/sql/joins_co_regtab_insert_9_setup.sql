-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_co_r;
DROP TABLE IF EXISTS dml_co_s;
DROP TABLE IF EXISTS dml_co_p;

CREATE TABLE dml_co_r (a int , b int default -1, c text) WITH (appendonly = true, orientation = column) DISTRIBUTED BY (a);
CREATE TABLE dml_co_p (a numeric, b decimal , c boolean , d text , e int) WITH (appendonly = true, orientation = column) DISTRIBUTED BY (a,b);
CREATE TABLE dml_co_s as select dml_co_r.b, dml_co_r.a, dml_co_r.c from dml_co_r, dml_co_p WHERE dml_co_r.a = dml_co_p.a;
ALTER TABLE dml_co_s SET DISTRIBUTED BY (b);

INSERT INTO dml_co_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_co_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_co_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_co_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_co_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_co_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_co_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_co_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_co_r VALUES(generate_series(1,10),NULL,'rn');
