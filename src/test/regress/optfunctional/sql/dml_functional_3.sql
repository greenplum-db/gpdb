
-- start_ignore
create schema functional_151_225;
set search_path to functional_151_225;
-- end_ignore
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Delete with join in USING (Delete all rows )



SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT 1)foo; 
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Delete with join in using



SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT 1 as t) foo WHERE foo.t = dml_heap_pt_r.a; 
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Delete with multiple joins



SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s,dml_heap_pt_p WHERE dml_heap_pt_r.a = dml_heap_pt_s.b and dml_heap_pt_r.b = dml_heap_pt_p.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Delete on table with composite distcol



SELECT COUNT(*) FROM dml_heap_pt_p;
DELETE FROM dml_heap_pt_p USING dml_heap_pt_r WHERE dml_heap_pt_p.b::int = dml_heap_pt_r.b::int and dml_heap_pt_p.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_p;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test15: Delete with PREPARE plan



SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b) foo;
SELECT COUNT(*) FROM dml_heap_pt_r;
PREPARE plan_del as DELETE FROM dml_heap_pt_r WHERE b in (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b);
EXECUTE plan_del;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test16: Delete with PREPARE plan



SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a)foo;
SELECT COUNT(*) FROM dml_heap_pt_s;
PREPARE plan_del_2 as DELETE FROM dml_heap_pt_s WHERE b in (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a);
EXECUTE plan_del_2;
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test17: Delete with sub-query



SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s WHERE a = (SELECT dml_heap_pt_r.a FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a);
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Delete with predicate



SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE a > 100;
DELETE FROM dml_heap_pt_s WHERE a > 100;
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE a > 100;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Delete with predicate



SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE a is NULL;
DELETE FROM dml_heap_pt_s WHERE a is NULL; 
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Delete with generate_series



SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s USING generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Delete with join on distcol



SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Delete with join on non-distribution column



SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Delete with join on non-distribution column



SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s USING dml_heap_pt_r WHERE dml_heap_pt_s.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Delete and using  



SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s; 
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Delete and using (with no rows)



TRUNCATE TABLE dml_heap_pt_s;
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s; 
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Negative test - Constant tuple Inserts( no partition for partitioning key )



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r values(10);
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Join on the distribution key of target table



SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_s.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a)foo;
INSERT INTO dml_heap_pt_s SELECT dml_heap_pt_s.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a;
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Join on distribution key of target table



SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.a = dml_heap_pt_s.a)foo;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.a = dml_heap_pt_s.a ;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Join on the distribution key of target table. Insert Large number of rows



SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_s SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.b = dml_heap_pt_s.b ;
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Join with different column order 



SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_s.a,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r(b,a,c) SELECT dml_heap_pt_s.a,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Join with Aggregate



SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_heap_pt_s.a, dml_heap_pt_r.b + dml_heap_pt_r.a ,'text' FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b GROUP BY dml_heap_pt_s.a,dml_heap_pt_r.b,dml_heap_pt_r.a)foo;
INSERT INTO dml_heap_pt_r SELECT COUNT(*) + dml_heap_pt_s.a, dml_heap_pt_r.b + dml_heap_pt_r.a ,'text' FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b GROUP BY dml_heap_pt_s.a,dml_heap_pt_r.b,dml_heap_pt_r.a ;
SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test15: Join with DISTINCT



SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_s.a = dml_heap_pt_r.a)foo;
INSERT INTO dml_heap_pt_s SELECT DISTINCT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_s.a = dml_heap_pt_r.a ;
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test16: Insert NULL with Joins



SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r SELECT NULL,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test18: Insert 0 rows



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test19: Insert 0 rows



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a and false;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Negative Test - Constant tuple Inserts( no partition )



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r values(NULL,NULL);
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type



SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT ('a')::int, dml_heap_pt_r.b,10 FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r SELECT ('a')::int, dml_heap_pt_r.b,10 FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test21: Negative test case. INSERT has more expressions than target columns



SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a GROUP BY dml_heap_pt_r.a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d)foo;
INSERT INTO dml_heap_pt_s SELECT COUNT(*) as a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_s.a = dml_heap_pt_r.a GROUP BY dml_heap_pt_r.a, dml_heap_pt_r.b, dml_heap_pt_r.c, dml_heap_pt_r.d;
SELECT COUNT(*) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test22: Insert with join on the partition key



SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.* FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test24: Negative test - Insert NULL value to a table without default partition



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.b, NULL, dml_heap_pt_r.c FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test25: Negative test - Insert out of partition range values for table without default partition



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.b ,dml_heap_pt_r.a + dml_heap_pt_s.a + 100, dml_heap_pt_r.c FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Negative Test - Multiple constant tuple Inserts



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL



SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
INSERT INTO dml_heap_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_heap_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_pt_r;
TRUNCATE TABLE dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT generate_series(1,10),1;
SELECT * FROM dml_heap_pt_r ORDER BY 1;
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_heap_pt_r;
	
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Insert with generate_series



SELECT COUNT(*) FROM dml_heap_pt_r;
INSERT INTO dml_heap_pt_r SELECT *,1 from generate_series(1,10);
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table



SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.a)foo;
INSERT INTO dml_heap_pt_r SELECT dml_heap_pt_r.* FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test1: Update to constant value



SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
UPDATE dml_heap_pt_r SET c = 'text';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='text';
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test10: Update distcol with predicate in subquery



UPDATE dml_heap_pt_r SET a = dml_heap_pt_r.a + 1 FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a and dml_heap_pt_s.b in (SELECT dml_heap_pt_s.b + dml_heap_pt_r.a FROM dml_heap_pt_s,dml_heap_pt_r WHERE dml_heap_pt_r.a > 10);
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test11: Update with aggregate in subquery 



SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = (SELECT COUNT(*) FROM dml_heap_pt_s);
SELECT COUNT(*) FROM dml_heap_pt_s;
UPDATE dml_heap_pt_s SET b = (SELECT COUNT(*) FROM dml_heap_pt_s) FROM dml_heap_pt_r WHERE dml_heap_pt_r.a = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = (SELECT COUNT(*) FROM dml_heap_pt_s);
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @description update_test12: Update and limit in subquery
-- @execute_all_plans True
-- @db_name dmldb



SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
SELECT DISTINCT(b) FROM dml_heap_pt_s ORDER BY 1 LIMIT 1;
UPDATE dml_heap_pt_r SET a = (SELECT DISTINCT(b) FROM dml_heap_pt_s ORDER BY 1 LIMIT 1) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test13: Update multiple columns 



SELECT COUNT(*) FROM dml_heap_pt_r WHERE b is NULL;
SELECT dml_heap_pt_s.a + 10 FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a ORDER BY 1 LIMIT 1;
SELECT * FROM dml_heap_pt_r WHERE a = 1;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_s.a + 10 ,b = NULL FROM dml_heap_pt_s WHERE dml_heap_pt_r.a + 2= dml_heap_pt_s.b;
SELECT * FROM dml_heap_pt_r WHERE a = 11 ORDER BY 1,2;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE b is NULL;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test14: Update multiple columns 



SELECT COUNT(*) FROM dml_heap_pt_r WHERE c='z';
SELECT dml_heap_pt_s.a ,dml_heap_pt_s.b,'z' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b ORDER BY 1,2 LIMIT 1;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET (a,b,c) = (dml_heap_pt_s.a ,dml_heap_pt_s.b,'z') FROM dml_heap_pt_s WHERE dml_heap_pt_r.a + 1= dml_heap_pt_s.b;
SELECT * FROM dml_heap_pt_r WHERE c='z' ORDER BY 1 LIMIT 1;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c='z';
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description update_test15: Update with prepare plans 



PREPARE plan_upd as UPDATE dml_heap_pt_r SET a = dml_heap_pt_s.a +1 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b ;
EXECUTE plan_upd;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test16: Update and case 



SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 20 ;
UPDATE dml_heap_pt_r SET a = (SELECT case when c = 'r' then MAX(b) else 100 end FROM dml_heap_pt_r GROUP BY c) ;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 20 ;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description update_test17: Negative test - Update with sub-query returning more than one row



SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = ( SELECT DISTINCT(b) FROM dml_heap_pt_s ) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT SUM(a) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description update_test18: Negative test - Update with sub-query returning more than one row



SELECT SUM(b) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET b = (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a );
SELECT SUM(b) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test19: Negative test - Update with aggregates 



SELECT SUM(b) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET b = MAX(dml_heap_pt_s.b) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT SUM(b) FROM dml_heap_pt_r;
-- start_ignore

DROP TABLE IF EXISTS dml_heap_pt_u;
DROP TABLE IF EXISTS dml_heap_pt_v;
CREATE TABLE dml_heap_pt_u as SELECT i as a, i as b  FROM generate_series(1,10)i;
CREATE TABLE dml_heap_pt_v as SELECT i as a, i as b FROM generate_series(1,10)i;
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test2: Update and set distribution key to constant



SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_pt_s)foo;
UPDATE dml_heap_pt_s SET b = 10;
SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_pt_s)foo;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test20: Negative test - Update WHERE join returns more than one tuple with different values.



SELECT SUM(a) FROM dml_heap_pt_v;
UPDATE dml_heap_pt_v SET a = dml_heap_pt_u.a FROM dml_heap_pt_u WHERE dml_heap_pt_u.b = dml_heap_pt_v.b;
SELECT SUM(a) FROM dml_heap_pt_v;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test21: Update with joins on multiple table



SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_r.b+1 FROM dml_heap_pt_p,dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.b and dml_heap_pt_r.a = dml_heap_pt_p.b+1;
SELECT SUM(a) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @skip MPP-19148
-- @description update_test22: Update on table wit composite distribution key



SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_p SET a = dml_heap_pt_p.b % 2 FROM dml_heap_pt_r WHERE dml_heap_pt_p.b::int = dml_heap_pt_r.b::int and dml_heap_pt_p.a = dml_heap_pt_r.a;
SELECT SUM(a) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test23: Update on table wit composite distribution key



SELECT SUM(b) FROM dml_heap_pt_p;
UPDATE dml_heap_pt_p SET b = (dml_heap_pt_p.b * 1.1)::int FROM dml_heap_pt_r WHERE dml_heap_pt_p.b = dml_heap_pt_r.a and dml_heap_pt_p.b = dml_heap_pt_r.b;
SELECT SUM(b) FROM dml_heap_pt_p;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test24: Update the partition key and move tuples across partitions( moving tuple to default partition)



SELECT SUM(a) FROM dml_heap_pt_s;
UPDATE dml_heap_pt_s SET a = dml_heap_pt_r.a + 30000 FROM dml_heap_pt_r WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT SUM(a) FROM dml_heap_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test25: Negative test update partition key (no default partition)



SELECT SUM(b) FROM dml_heap_pt_r; 
UPDATE dml_heap_pt_r SET b = dml_heap_pt_r.a + 3000000 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT SUM(b) FROM dml_heap_pt_r; 
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test3: Update to default value 



SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = DEFAULT; 
SELECT SUM(a) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test4: Update to default value



SELECT SUM(a) FROM dml_heap_pt_r;
SELECT SUM(b) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET a = DEFAULT, b = DEFAULT; 
SELECT SUM(a) FROM dml_heap_pt_r;
SELECT SUM(b) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test5: Update and reset the value



SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_pt_r)foo;
UPDATE dml_heap_pt_r SET a = a;
SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_pt_r)foo;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test6: Update and generate_series




SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='n';
UPDATE dml_heap_pt_r SET a = generate_series(1,10), c ='n';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='n';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test7: Update distcol where join on target table non dist key



SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_r.a + 1 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT SUM(a) FROM dml_heap_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test8: Update and from values



SELECT * FROM dml_heap_pt_r WHERE b = 20 ORDER BY 1;
UPDATE dml_heap_pt_r SET a = v.i + 1 FROM (VALUES(100, 20)) as v(i, j) WHERE dml_heap_pt_r.b = v.j;
SELECT * FROM dml_heap_pt_r WHERE b = 20 ORDER BY 1;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_heap_pt_r;
DROP TABLE IF EXISTS dml_heap_pt_s;
DROP TABLE IF EXISTS dml_heap_pt_p;

CREATE TABLE dml_heap_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_heap_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(101) every(10),
	default partition def);

CREATE TABLE dml_heap_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);


INSERT INTO dml_heap_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_heap_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_heap_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_heap_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_heap_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test9: Update with Joins and set to constant value



SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = 10;
UPDATE dml_heap_pt_s SET b = 10 FROM dml_heap_pt_r WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = 10;
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
-- @description test1: Constant tuple Inserts



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(10);
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
-- @description test10: Join on the distribution key of target table



SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT dml_heap_s.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a)foo;
INSERT INTO dml_heap_s SELECT dml_heap_s.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_s.a = dml_heap_r.a;
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
-- @description test11: Join on distribution key of target table



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.a = dml_heap_s.a)foo;
INSERT INTO dml_heap_r SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.a = dml_heap_s.a ;
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
-- @description test12: Join on the distribution key of target table. Insert Large number of rows



SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.b <> dml_heap_s.b )foo;
INSERT INTO dml_heap_s SELECT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_r.b <> dml_heap_s.b ;
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
-- @description test13: Join with different column order 



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT dml_heap_s.a,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r(b,a,c) SELECT dml_heap_s.a,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s  WHERE dml_heap_r.b = dml_heap_s.b;
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
-- @description test14: Join with Aggregate



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_heap_s.a, dml_heap_r.b + dml_heap_r.a ,'text' FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b GROUP BY dml_heap_s.a,dml_heap_r.b,dml_heap_r.a)foo;
INSERT INTO dml_heap_r SELECT COUNT(*) + dml_heap_s.a, dml_heap_r.b + dml_heap_r.a ,'text' FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b GROUP BY dml_heap_s.a,dml_heap_r.b,dml_heap_r.a ;
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
-- @description test15: Join with DISTINCT



SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_s.a = dml_heap_r.a)foo;
INSERT INTO dml_heap_s SELECT DISTINCT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_s.a = dml_heap_r.a ;
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
-- @description test16: Insert NULL with Joins



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b)foo;
INSERT INTO dml_heap_r SELECT NULL,dml_heap_r.b,'text' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b;
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
-- @description test17: Insert and CASE



SELECT COUNT(*) FROM dml_heap_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_heap_p.a) as A, dml_heap_p.a as B, dml_heap_s.c as C FROM dml_heap_p, dml_heap_s WHERE dml_heap_p.a = dml_heap_s.a GROUP BY dml_heap_p.a,dml_heap_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_heap_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_heap_p.a) as A, dml_heap_p.a as B, dml_heap_s.c as C FROM dml_heap_p, dml_heap_s WHERE dml_heap_p.a = dml_heap_s.a GROUP BY dml_heap_p.a,dml_heap_s.c)as x GROUP BY A,B,C;
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
-- @description test18: Insert 0 rows



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a LIMIT 0;
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
-- @description test19: Insert 0 rows



SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r SELECT dml_heap_r.* FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a and false;
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
-- @description test2: Constant tuple Inserts

SELECT COUNT(*) FROM dml_heap_r;
INSERT INTO dml_heap_r values(NULL);
SELECT COUNT(*) FROM dml_heap_r;

-- start_ignore
drop schema functional_151_225 cascade;
-- end_ignore
