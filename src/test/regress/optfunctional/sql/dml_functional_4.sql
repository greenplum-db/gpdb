
-- start_ignore
create schema functional_226_300;
set search_path to functional_226_300;
-- end_ignore
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM ( SELECT ('a')::int, dml_heap_r.b,10 FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r SELECT ('a')::int, dml_heap_r.b,10 FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test21: Negative test case. INSERT has more expressions than target columns



SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_heap_r.* FROM dml_heap_r, dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a GROUP BY dml_heap_r.a, dml_heap_r.b, dml_heap_r.c)foo;
INSERT INTO dml_heap_s SELECT COUNT(*) as a, dml_heap_r.* FROM dml_heap_r, dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a GROUP BY dml_heap_r.a, dml_heap_r.b, dml_heap_r.c;
SELECT COUNT(*) FROM dml_heap_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Multiple constant tuple Inserts



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_heap_r WHERE c ='text' ORDER BY 1;
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_r;
TRUNCATE TABLE dml_heap_r;
INSERT INTO dml_heap_r SELECT generate_series(1,10);
SELECT * FROM dml_heap_r ORDER BY 1;
SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

SELECT truncate_dml_heap_r(); 
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_r;
	
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT * from generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test1: Update to constant value



SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
UPDATE dml_heap_r SET c = 'text';
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test10: Update distcol with predicate in subquery



UPDATE dml_heap_r SET a = dml_heap_r.a + 1 FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a and dml_heap_s.b in (SELECT dml_heap_s.b + dml_heap_r.a FROM dml_heap_s,dml_heap_r WHERE dml_heap_r.a > 10);
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test11: Update with aggregate in subquery 



SELECT COUNT(*) FROM dml_heap_s WHERE b = (SELECT COUNT(*) FROM dml_heap_s);
UPDATE dml_heap_s SET b = (SELECT COUNT(*) FROM dml_heap_s) FROM dml_heap_r WHERE dml_heap_r.a = dml_heap_s.b;
SELECT COUNT(*) FROM dml_heap_s WHERE b = (SELECT COUNT(*) FROM dml_heap_s);
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test12: Update and limit in subquery



SELECT COUNT(*) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = (SELECT DISTINCT(b) FROM dml_heap_s ORDER BY 1 LIMIT 1) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test13: Update multiple columns 



SELECT COUNT(*) FROM dml_heap_r WHERE b is NULL;
SELECT dml_heap_s.a + 10 FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a ORDER BY 1 LIMIT 1;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = dml_heap_s.a + 10 ,b = NULL FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_r WHERE b is NULL;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test14: Update multiple columns 



SELECT COUNT(*) FROM dml_heap_r WHERE c='z';
SELECT dml_heap_s.a ,dml_heap_s.b,'z' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b ORDER BY 1,2 LIMIT 1;
UPDATE dml_heap_r SET (a,b,c) = (dml_heap_s.a ,dml_heap_s.b,'z') FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b;
SELECT * FROM dml_heap_r WHERE c='z' ORDER BY 1 LIMIT 1;
SELECT COUNT(*) FROM dml_heap_r WHERE c='z';
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description update_test15: Update with prepare plans 



PREPARE plan_upd as UPDATE dml_heap_r SET a = dml_heap_s.a +1 FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b ;
EXECUTE plan_upd;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test16: Update and case 



SELECT COUNT(*) FROM dml_heap_r WHERE a = 100 ;
UPDATE dml_heap_r SET a = (SELECT case when c = 'r' then MAX(b) else 100 end FROM dml_heap_r GROUP BY c ORDER BY 1 LIMIT 1) ;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 100 ;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description update_test17: Negative test - Update with sub-query returning more than one row



SELECT SUM(a) FROM dml_heap_r;
UPDATE dml_heap_r SET a = ( SELECT DISTINCT(b) FROM dml_heap_s ) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description update_test18: Negative test - Update with sub-query returning more than one row



SELECT SUM(b) FROM dml_heap_r;
UPDATE dml_heap_r SET b = (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a );
SELECT SUM(b) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description update_test19: Negative test - Update with aggregates 



SELECT SUM(b) FROM dml_heap_r;
UPDATE dml_heap_r SET b = MAX(dml_heap_s.b) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT SUM(b) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test2: Update and set distribution key to constant



SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_s)foo;
UPDATE dml_heap_s SET b = 10;
SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_s)foo;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description update_test20: Negative test - Update WHERE join returns more than one tuple with different values.



DROP TABLE IF EXISTS dml_heap_u;
DROP TABLE IF EXISTS dml_heap_v;
CREATE TABLE dml_heap_u as SELECT i as a, 1 as b  FROM generate_series(1,10)i;
CREATE TABLE dml_heap_v as SELECT i as a ,i as b FROM generate_series(1,10)i;
SELECT SUM(a) FROM dml_heap_v;
UPDATE dml_heap_v SET a = dml_heap_u.a FROM dml_heap_u WHERE dml_heap_u.b = dml_heap_v.b;
SELECT SUM(a) FROM dml_heap_v;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test21: Update with joins on multiple table



UPDATE dml_heap_r SET a = dml_heap_r.b+1 FROM dml_heap_p,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b and dml_heap_r.a = dml_heap_p.b+1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test22: Update on table wit composite distribution key



UPDATE dml_heap_p SET a = dml_heap_p.b % 2 FROM dml_heap_r WHERE dml_heap_p.b::int = dml_heap_r.b::int and dml_heap_p.a = dml_heap_r.a;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test23: Update on table wit composite distribution key



UPDATE dml_heap_p SET b = (dml_heap_p.b * 1.1)::int FROM dml_heap_r WHERE dml_heap_p.b = dml_heap_r.a and dml_heap_p.b = dml_heap_r.b;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test3: Update to default value 



SELECT SUM(a) FROM dml_heap_r;
UPDATE dml_heap_r SET a = DEFAULT; 
SELECT SUM(a) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test4: Update to default value



SELECT SUM(a) FROM dml_heap_r;
SELECT SUM(b) FROM dml_heap_r;
UPDATE dml_heap_r SET a = DEFAULT, b = DEFAULT; 
SELECT SUM(a) FROM dml_heap_r;
SELECT SUM(b) FROM dml_heap_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test5: Update and reset the value



SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_r)foo;
UPDATE dml_heap_r SET a = a;
SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_r)foo;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test6: Update and generate_series



SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_r WHERE c ='n';
UPDATE dml_heap_r SET a = generate_series(1,10), c ='n';
SELECT COUNT(*) FROM dml_heap_r WHERE c ='n';
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test7: Update distcol where join on target table non dist key



SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = dml_heap_r.a + 1 FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test8: Update and from values



SELECT SUM(b) FROM dml_heap_r WHERE b = 20;
UPDATE dml_heap_r SET a = v.i + 1 FROM (VALUES(100, 20)) as v(i, j) WHERE dml_heap_r.b = v.j;
SELECT SUM(b) FROM dml_heap_r WHERE b = 20;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_r;
DROP TABLE IF EXISTS dml_heap_s;
DROP TABLE IF EXISTS dml_heap_p;

CREATE TABLE dml_heap_r (a int , b int default -1, c text) DISTRIBUTED BY (a);
CREATE TABLE dml_heap_p (a numeric, b decimal , c boolean , d text , e int) DISTRIBUTED BY (a,b);
CREATE TABLE dml_heap_s as select dml_heap_r.b, dml_heap_r.a, dml_heap_r.c from dml_heap_r, dml_heap_p WHERE dml_heap_r.a = dml_heap_p.a;
ALTER TABLE dml_heap_s SET DISTRIBUTED BY (b);

INSERT INTO dml_heap_p SELECT id * 1.012, id * 1.1, true, 'new', id as d FROM (SELECT * FROM generate_series(1,100) as id) AS x;
INSERT INTO dml_heap_p VALUES(generate_series(1,10),NULL,false,'pn',NULL);
INSERT INTO dml_heap_p VALUES(NULL,1,false,'pn',NULL),(1,NULL,false,'pn',0),(NULL,NULL,false,'pn',0);

INSERT INTO dml_heap_s VALUES(generate_series(1,100),generate_series(1,100),'s');
INSERT INTO dml_heap_s VALUES(NULL,NULL,'sn'),(1,NULL,'sn'),(NULL,0,'sn');
INSERT INTO dml_heap_s VALUES(generate_series(1,10),NULL,'sn');

INSERT INTO dml_heap_r VALUES(generate_series(1,100),generate_series(1,100),'r');
INSERT INTO dml_heap_r VALUES(NULL,NULL,'rn'),(1,NULL,'rn'),(NULL,0,'rn');
INSERT INTO dml_heap_r VALUES(generate_series(1,10),NULL,'rn');
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test9: Update with Joins and set to constant value



SELECT COUNT(*) FROM dml_heap_s WHERE b = 10;
UPDATE dml_heap_s SET b = 10 FROM dml_heap_r WHERE dml_heap_r.b = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_s WHERE b = 10;
-- start_ignore

DROP TABLE IF EXISTS a;
CREATE TABLE a( a int) partition by range(a)(start(1) end(10) every(1),default partition a);
INSERT INTO a SELECT generate_series(1,10);

DROP TABLE IF EXISTS e;
DROP TABLE IF EXISTS f;
CREATE TABLE e( b int,a int) with (appendonly = true) partition by range(a)(start(1) end(10) every(1),default partition def);
CREATE TABLE f(b int, a int) with (appendonly = true, orientation=column) partition by range(a)(start(1) end(10) every(1),default partition def);
INSERT INTO f SELECT i,i FROM generate_series(1,10)i;
INSERT INTO e SELECT i,i FROM generate_series(1,10)i;
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description MPP-19227 Check the error message on AO/CO tables when we issue updates




UPDATE f SET a = NULL from a where a.a = f.a;
UPDATE e SET a = NULL from a where a.a = e.a;
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, x int, y int ) distributed randomly;

DROP TABLE IF EXISTS r_p;
CREATE TABLE r_p ( a int, b int, x int, y int ) distributed randomly
partition by list(a) ( VALUES (0), VALUES (1) );

DROP TABLE IF EXISTS s;
CREATE TABLE s ( c int, d int, e int ) distributed randomly;

INSERT INTO s VALUES
(0,0,0),(0,0,1),(0,1,0),(0,1,1),(1,0,0),(1,0,1),(1,1,0),(1,1,1);

INSERT INTO r VALUES
    (0, 0, 1, 1),
    (0, 1, 2, 2),
    (0, 1, 2, 2),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4);

INSERT INTO r_p VALUES
    (0, 0, 1, 1),
    (0, 1, 2, 2),
    (0, 1, 2, 2),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @description MPP-20083




SELECT COUNT(*) 
FROM r, s c, s d, s e
WHERE r.a = c.c AND r.a = d.d AND r.a = e.e;

SELECT COUNT(*)
FROM r_p, s c, s d, s e
WHERE r_p.a = c.c AND r_p.a = d.d AND r_p.a = e.e;
-- start_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description Mpp-20207

DROP TABLE if exists altable;
CREATE TABLE altable(a int, b text, c int);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description Mpp-20207



ALTER TABLE altable DROP COLUMN b;

ALTER TABLE altable ADD CONSTRAINT c_check CHECK (c > 0);
INSERT INTO altable(a, c) VALUES(0, 10);
SELECT * FROM altable ORDER BY 1;
DELETE FROM altable WHERE c = -10;
SELECT * FROM altable ORDER BY 1;
DELETE FROM altable WHERE c = 10;
SELECT * FROM altable ORDER BY 1;
DELETE FROM altable WHERE c = 10;
SELECT * FROM altable ORDER BY 1;

-- start_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description Mpp-20207

DROP TABLE if exists altable;
CREATE TABLE altable(a int, b text, c int);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @description Mpp-20207



ALTER TABLE altable DROP COLUMN b;

ALTER TABLE altable ADD CONSTRAINT c_check CHECK (c > 0);
INSERT INTO altable(a, c) VALUES(0, -10);
SELECT * FROM altable ORDER BY 1;
INSERT INTO altable(a, c) VALUES(0, 10);
SELECT * FROM altable ORDER BY 1;

-- start_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description Mpp-20207

DROP TABLE if exists altable;
CREATE TABLE altable(a int, b text, c int);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @description Mpp-20207



ALTER TABLE altable DROP COLUMN b;

ALTER TABLE altable ADD CONSTRAINT c_check CHECK (c > 0);
INSERT INTO altable(a, c) VALUES(0, 10);
SELECT * FROM altable ORDER BY 1;
UPDATE altable SET c = -10;
SELECT * FROM altable ORDER BY 1;
UPDATE altable SET c = 1;
SELECT * FROM altable ORDER BY 1;
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int) distributed by (a);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description direct dispatch test



INSERT INTO r VALUES (1,1,1);
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int) distributed by (a);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description direct dispatch test



INSERT INTO r VALUES (default,default,default);
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description Negative test for direct dispatch



INSERT INTO r VALUES (1,1,1),(3,3,3);
-- start_ignore

DROP TABLE IF EXISTS a;
DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int) distributed by (a);
CREATE TABLE a (a int) distributed by (a);
INSERT INTO a VALUES(1);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description Negative test for direct dispatch. insert with select



INSERT INTO r select 1,a,1 from a;
-- start_ignore

DROP TABLE IF EXISTS m; 
CREATE TABLE m ();
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
ALTER TABLE m ADD column a integer default null;
ALTER TABLE m ADD column b integer default null;
ALTER TABLE m ADD column c integer default null;
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description Negative test for direct dispatch. Master only table



INSERT INTO m VALUES(1,1,1);
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int) distributed randomly;
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description Negative test for direct dispatch. Randomly distributed table



INSERT INTO r VALUES (1,1,1);
INSERT INTO r VALUES (1,1,1),(3,3,3);
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int);
INSERT INTO r VALUES(1,1,1);
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description update and direct dispatch 



UPDATE r SET a = 10;
SELECT * from r;
-- start_ignore

DROP TABLE IF EXISTS r;
CREATE TABLE r ( a int, b int, c int) distributed by (a);
INSERT INTO r VALUES(1,1,1);
-- end_ignore
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @product_version gpdb: [4.3-]
-- @gucs gp_autostats_mode=none;test_print_direct_dispatch_info=true
-- @db_name dmldb
-- @description delete and direct dispatch 



DELETE FROM r WHERE a = 10;
SELECT * from r;
-- start_ignore

drop table if exists m;
drop table if exists zzz;

create table zzz(a int primary key, b int) distributed by (a);

-- create master-only table
create table m(); 
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into zzz select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




delete from zzz where a in (select a from m);

select * from zzz order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists zzz;

create table zzz(a int primary key, b int) distributed by (a);

-- create master-only table
create table m(); 
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into zzz select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




delete from zzz where b in (select a from m);

select * from zzz order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists yyy;

create table yyy(a int, b int) distributed randomly;

-- create master-only table
create table m(); 
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into yyy select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




delete from yyy where a in (select a from m);

select * from yyy order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists yyy;

create table yyy(a int, b int) distributed randomly;

-- create master-only table
create table m(); 
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into yyy select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




delete from yyy where b in (select a from m);

select * from yyy order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists zzz;

create table zzz(a int primary key, b int) distributed by (a);
-- create master-only table
create table m(); 
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




insert into zzz select a,b from m;

select * from zzz order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists yyy;

create table yyy(a int, b int) distributed randomly;
-- create master-only table
create table m(); 
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




insert into yyy select a,b from m;

select * from yyy order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists zzz;

create table zzz(a int primary key, b int) distributed by (a);

-- create master-only table
create table m();
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%3 from generate_series(3,10)i;

insert into zzz select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on


set optimizer_enable_master_only_queries=on;


update zzz set a=m.b from m where m.a=zzz.b;

select * from zzz order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists zzz;

create table zzz(a int primary key, b int) distributed by (a);

-- create master-only table
create table m();
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into zzz select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




update zzz set b=m.b from m where m.a=zzz.a;

select * from zzz order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists yyy;

create table yyy(a int, b int) distributed randomly;

-- create master-only table
create table m(); 
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into yyy select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




update yyy set a=m.b from m where m.a=yyy.b;

select * from yyy order by 1, 2;
-- start_ignore

drop table if exists m;
drop table if exists yyy;

create table yyy(a int, b int) distributed randomly;

-- create master-only table
create table m(); 
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;

insert into yyy select a,b from m;
-- end_ignore
-- @author ramans2
-- @created 2013-10-30 12:00:00 
-- @modified 2013-10-30 12:00:00
-- @tags dml 
-- @db_name dmldb
-- @description MPP-21536: Duplicated rows inserted when ORCA is turned on




update yyy set b=m.b from m where m.a=yyy.a;

select * from yyy order by 1, 2;
-- start_ignore

drop table if exists r;
create table r (a int primary key, b int) distributed by (a);
insert into r values(1,1);
-- end_ignore
-- @author prabhd
-- @created 2014-03-10 12:00:00
-- @modified 2014-03-10 12:00:00
-- @tags dml MPP-21622
-- @db_name dmldb
-- @product_version gpdb: [4.3.0.0-] 
-- @optimizer_mode on
-- @description MPP-21622 Update with primary key: only sort if the primary key is updated




--start_ignore
explain update r set b = 5;
--end_ignore
update r set b = 5;
select * from r order by 1,2;

--start_ignore
explain update r set a = 5;
--end_ignore
update r set a = 5;
select * from r order by 1,2;
-- start_ignore
drop table if exists orca;
create table orca (a int not null);
-- end_ignore
-- @author prabhd
-- @created 2014-02-25 12:00:00
-- @modified 2014-02-25 12:00:00
-- @tags dml MPP-22046
-- @db_name dmldb
-- @gpopt 1.532
-- @gucs client_min_messages='log';gp_log_gang="verbose"
-- @optimizer_mode on
-- @description MPP-22046
insert into orca values(null);        
-- start_ignore
drop table if exists dml_multi_pt_update;
create table dml_multi_pt_update(a int, b int, c int,d int )
distributed by (a)
partition by range(b)
subpartition by range(c) 
subpartition template (default subpartition subothers,start (1) end(7) every (4) )
(default partition others, start(1) end(5) every(3));
-- end_ignore
-- @author prabhd
-- @created 2014-02-24 12:00:00
-- @modified 2014-02-24 12:00:00
-- @tags dml
-- @optimizer_mode=on
-- @db_name dmldb
-- @gucs client_min_messages='log'
-- @description MPP-22599 DML queries that fallback to planner don't check for updates on the distribution key. This test will fail once Multi-level partitioned tables are supported in ORCA.
update dml_multi_pt_update set a = 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test1:  With UNION/INTERSECT/EXCEPT in dml_union_subquery




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a in (SELECT a FROM dml_union_r UNION SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test10:  With UNION/INTERSECT/EXCEPT in the predicate condition




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT a FROM dml_union_r UNION SELECT a FROM dml_union_s ORDER BY a LIMIT 1);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test11:  With UNION/INTERSECT/EXCEPT in the predicate condition




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT a FROM dml_union_r INTERSECT SELECT a FROM dml_union_s ORDER BY a LIMIT 1);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test12:  With UNION/INTERSECT/EXCEPT in the predicate condition




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT a FROM dml_union_r EXCEPT SELECT a FROM dml_union_s ORDER BY a LIMIT 1);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test13:  With UNION/INTERSECT/EXCEPT 




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r USING (SELECT a FROM dml_union_r UNION SELECT a FROM dml_union_s) foo WHERE foo.a = dml_union_r.a;
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test14:  With UNION/INTERSECT/EXCEPT 




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r USING (SELECT a FROM dml_union_r INTERSECT SELECT a FROM dml_union_s) foo WHERE foo.a = dml_union_r.a;
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test15:  With UNION/INTERSECT/EXCEPT 




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r USING (SELECT a FROM dml_union_r EXCEPT SELECT a FROM dml_union_s) foo WHERE foo.a = dml_union_r.a;
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test16:  Using Partition table 




SELECT COUNT(*) FROM dml_union_s;
DELETE FROM dml_union_s USING (SELECT a FROM dml_union_r UNION SELECT a FROM dml_union_s_1_prt_def) foo;
SELECT COUNT(*) FROM dml_union_s;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test17:  Using Partition table 




SELECT COUNT(*) FROM dml_union_s;
DELETE FROM dml_union_s USING (SELECT * FROM dml_union_r UNION SELECT * FROM dml_union_s_1_prt_def) foo WHERE foo.d = dml_union_s.d;
SELECT COUNT(*) FROM dml_union_s;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test2:  With UNION/INTERSECT/EXCEPT in dml_union_subquery




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a in (SELECT a FROM dml_union_r UNION ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test3:  With UNION/INTERSECT/EXCEPT in dml_union_subquery




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a in (SELECT a FROM dml_union_r INTERSECT SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test4:  With UNION/INTERSECT/EXCEPT in dml_union_subquery




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a in (SELECT a FROM dml_union_r INTERSECT ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test5:  With UNION/INTERSECT/EXCEPT in dml_union_subquery




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a in (SELECT a FROM dml_union_r EXCEPT SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test6:  With UNION/INTERSECT/EXCEPT in dml_union_subquery




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a in (SELECT a FROM dml_union_r EXCEPT ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test7:  With UNION/INTERSECT/EXCEPT in the predicate condition ( 0 rows)




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT NULL UNION SELECT NULL); 
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);



INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test8:  With UNION/INTERSECT/EXCEPT in the predicate condition ( 0 rows )




SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT NULL INTERSECT SELECT NULL);
SELECT COUNT(*) FROM dml_union_r;

-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_union_r;
DROP TABLE IF EXISTS dml_union_s;

CREATE TABLE dml_union_r (
        a int CONSTRAINT r_check_a CHECK(a <> -1),
        b int,
        c text,
        d numeric)
DISTRIBUTED BY (a);

CREATE TABLE dml_union_s (
        a int ,
        b int not NULL,
        c text ,
        d numeric default 10.00)
DISTRIBUTED BY (b)
PARTITION BY range(d) (
        start(1) end(1001) every(100),
        default partition def
);

INSERT INTO dml_union_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_union_r VALUES(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL),(NULL,NULL,'text',NULL);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_r VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);

INSERT INTO dml_union_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) ;
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,1,'text',1),(2,2,'text',2),(3,3,'text',3),(4,4,'text',4),(5,5,'text',5);
INSERT INTO dml_union_s VALUES(1,2,'text',3),(2,3,'text',4),(3,4,'text',5),(4,5,'text',6),(5,6,'text',7);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description union_delete_test9:  With UNION/INTERSECT/EXCEPT in the predicate condition( 0 rows )

SELECT COUNT(*) FROM dml_union_r;
DELETE FROM dml_union_r WHERE a = (SELECT NULL EXCEPT SELECT NULL); 
SELECT COUNT(*) FROM dml_union_r;


-- start_ignore
drop schema functional_226_300 cascade;
-- end_ignore
