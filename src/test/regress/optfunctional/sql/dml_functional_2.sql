
-- start_ignore
create schema functional_76_150;
set search_path to functional_76_150;
-- end_ignore
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_co_check_p;
DROP TABLE IF EXISTS dml_co_check_r;
DROP TABLE IF EXISTS dml_co_check_s;

CREATE TABLE dml_co_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_co_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_co_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_co_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_co_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_co_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_co_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint



SELECT COUNT(*) FROM dml_co_check_r;
SELECT COUNT(*) FROM (SELECT dml_co_check_r.a + 110 , dml_co_check_r.b, dml_co_check_r.c, dml_co_check_r.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a)foo;
INSERT INTO dml_co_check_r SELECT dml_co_check_r.a + 110 , dml_co_check_r.b, dml_co_check_r.c, dml_co_check_r.d FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a;
SELECT COUNT(*) FROM dml_co_check_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_co_check_p;
DROP TABLE IF EXISTS dml_co_check_r;
DROP TABLE IF EXISTS dml_co_check_s;

CREATE TABLE dml_co_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_co_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_co_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_co_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_co_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_co_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_co_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_co_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test5: Insert with joins where the result tuples violate violates multiple check constraints



SELECT COUNT(*) FROM dml_co_check_r;
SELECT COUNT(*) FROM (SELECT dml_co_check_r.a + 110 , 0, dml_co_check_r.c, NULL FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a)foo;
INSERT INTO dml_co_check_r SELECT dml_co_check_r.a + 110 , 0, dml_co_check_r.c, NULL FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a;
SELECT COUNT(*) FROM dml_co_check_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Negative test - Constant tuple Inserts( no partition for partitioning key )



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(10);
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Join on the distribution key of target table



SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT dml_co_pt_s.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a)foo;
INSERT INTO dml_co_pt_s SELECT dml_co_pt_s.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a;
SELECT COUNT(*) FROM dml_co_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Join on distribution key of target table



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.a = dml_co_pt_s.a)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.a = dml_co_pt_s.a ;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Join on the distribution key of target table. Insert Large number of rows



SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_s SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.b = dml_co_pt_s.b ;
SELECT COUNT(*) FROM dml_co_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Join with different column order 



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_s.a,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r(b,a,c) SELECT dml_co_pt_s.a,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Join with Aggregate



SELECT COUNT(*) FROM dml_co_pt_r;
ALTER TABLE dml_co_pt_r ADD DEFAULT partition def;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_co_pt_s.a, dml_co_pt_r.b + dml_co_pt_r.a ,'text' FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b GROUP BY dml_co_pt_s.a,dml_co_pt_r.b,dml_co_pt_r.a)foo;
INSERT INTO dml_co_pt_r SELECT COUNT(*) + dml_co_pt_s.a, dml_co_pt_r.b + dml_co_pt_r.a ,'text' FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b GROUP BY dml_co_pt_s.a,dml_co_pt_r.b,dml_co_pt_r.a ;
SELECT COUNT(*) FROM dml_co_pt_r;
ALTER TABLE dml_co_pt_r DROP DEFAULT partition;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test15: Join with DISTINCT



SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_s.a = dml_co_pt_r.a)foo;
INSERT INTO dml_co_pt_s SELECT DISTINCT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_s.a = dml_co_pt_r.a ;
SELECT COUNT(*) FROM dml_co_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test16: Insert NULL with Joins



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT NULL,dml_co_pt_r.b,'text' FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test18: Insert 0 rows



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test19: Insert 0 rows



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.a and false;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Negative Test - Constant tuple Inserts( no partition )



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(NULL,NULL);
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT ('a')::int, dml_co_pt_r.b,10 FROM dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT ('a')::int, dml_co_pt_r.b,10 FROM dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test21: Negative test case. INSERT has more expressions than target columns



SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a GROUP BY dml_co_pt_r.a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d)foo;
INSERT INTO dml_co_pt_s SELECT COUNT(*) as a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a GROUP BY dml_co_pt_r.a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d;
SELECT COUNT(*) FROM dml_co_pt_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test22: Insert with join on the partition key



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.* FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test24: Negative test - Insert NULL value to a table without default partition



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.b, NULL, dml_co_pt_r.c FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.b, NULL, dml_co_pt_r.c FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.b = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test25: Negative test - Insert out of partition range values for table without default partition



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.b ,dml_co_pt_r.a + dml_co_pt_s.a + 100, dml_co_pt_r.c FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_r.a = dml_co_pt_s.b;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Negative Test - Multiple constant tuple Inserts



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Insert with generate_series



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_co_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL



SELECT COUNT(*) FROM dml_co_pt_r;
ALTER TABLE dml_co_pt_r ADD DEFAULT partition def;
INSERT INTO dml_co_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_co_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_co_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Insert with generate_series



SELECT COUNT(*) FROM dml_co_pt_r;
TRUNCATE TABLE dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT generate_series(1,10),1;
SELECT * FROM dml_co_pt_r ORDER BY 1;
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Insert with generate_series



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_co_pt_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_pt_r;
	
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Insert with generate_series



SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r SELECT *,1 from generate_series(1,10);
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

DROP TABLE IF EXISTS dml_co_pt_r;
DROP TABLE IF EXISTS dml_co_pt_s;
DROP TABLE IF EXISTS dml_co_pt_p;

CREATE TABLE dml_co_pt_r (
	a int ,
	b int ,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a)
partition by range(b) (
	start(1) end(301) every(10));

CREATE TABLE dml_co_pt_s (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (b)
partition by range(a) (
	start(1) end(100) every(10),
	default partition def);

CREATE TABLE dml_co_pt_p (
	a int ,
	b int,
	c text ,
	d numeric)
WITH (appendonly = true, orientation = column)
DISTRIBUTED BY (a,b)
partition by list(a,d) (
	partition one VALUES ((1,1),(1,2),(1,3),(1,4),(1,5)),
	partition two VALUES((2,1),(2,2),(2,3),(2,4),(2,5)),
	default partition def);

INSERT INTO dml_co_pt_r SELECT generate_series(1,100), generate_series(1,100) * 3,'r', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p SELECT generate_series(1,100), generate_series(1,100) * 3,'p', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_p VALUES(generate_series(1,10),NULL,'pn',NULL);
INSERT INTO dml_co_pt_p VALUES(NULL,1,'pn',NULL),(1,NULL,'pn',0),(NULL,NULL,'pn',0),(0,1,'pn',NULL),(NULL,NULL,'pn',NULL);
INSERT INTO dml_co_pt_s SELECT generate_series(1,100), generate_series(1,100) * 3,'s', generate_series(1,100) % 6;
INSERT INTO dml_co_pt_s VALUES(generate_series(1,10),NULL,'sn',NULL);
INSERT INTO dml_co_pt_s VALUES(NULL,1,'sn',NULL),(1,NULL,'sn',0),(NULL,NULL,'sn',0),(0,1,'sn',NULL),(NULL,NULL,'sn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table



SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.a)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.* FROM dml_co_pt_r,dml_co_pt_s  WHERE dml_co_pt_r.b = dml_co_pt_s.a;
SELECT COUNT(*) FROM dml_co_pt_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Constant tuple Inserts



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(10);
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Join on the distribution key of target table



SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT dml_co_s.* FROM dml_co_r,dml_co_s WHERE dml_co_s.a = dml_co_r.a)foo;
INSERT INTO dml_co_s SELECT dml_co_s.* FROM dml_co_r,dml_co_s WHERE dml_co_s.a = dml_co_r.a;
SELECT COUNT(*) FROM dml_co_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Join on distribution key of target table



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.a = dml_co_s.a)foo;
INSERT INTO dml_co_r SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.a = dml_co_s.a ;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Join on the distribution key of target table. Insert Large number of rows



SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.b <> dml_co_s.b )foo;
INSERT INTO dml_co_s SELECT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_r.b <> dml_co_s.b ;
SELECT COUNT(*) FROM dml_co_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Join with different column order 



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT dml_co_s.a,dml_co_r.b,'text' FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r(b,a,c) SELECT dml_co_s.a,dml_co_r.b,'text' FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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

-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Join with Aggregate



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_co_s.a, dml_co_r.b + dml_co_r.a ,'text' FROM dml_co_r, dml_co_s WHERE dml_co_r.b = dml_co_s.b GROUP BY dml_co_s.a,dml_co_r.b,dml_co_r.a)foo;
INSERT INTO dml_co_r SELECT COUNT(*) + dml_co_s.a, dml_co_r.b + dml_co_r.a ,'text' FROM dml_co_r, dml_co_s WHERE dml_co_r.b = dml_co_s.b GROUP BY dml_co_s.a,dml_co_r.b,dml_co_r.a ;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test15: Join with DISTINCT



SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_s.a = dml_co_r.a)foo;
INSERT INTO dml_co_s SELECT DISTINCT dml_co_r.a,dml_co_r.b,dml_co_s.c FROM dml_co_s INNER JOIN dml_co_r on dml_co_s.a = dml_co_r.a ;
SELECT COUNT(*) FROM dml_co_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test16: Insert NULL with Joins



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT NULL,dml_co_r.b,'text' FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT NULL,dml_co_r.b,'text' FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test17: Insert and CASE



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_co_p.a) as A, dml_co_p.a as B, dml_co_s.c as C FROM dml_co_p, dml_co_s WHERE dml_co_p.a = dml_co_s.a GROUP BY dml_co_p.a,dml_co_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_co_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_co_p.a) as A, dml_co_p.a as B, dml_co_s.c as C FROM dml_co_p, dml_co_s WHERE dml_co_p.a = dml_co_s.a GROUP BY dml_co_p.a,dml_co_s.c)as x GROUP BY A,B,C;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test18: Insert 0 rows



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.a LIMIT 0;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test19: Insert 0 rows



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.a and false;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Constant tuple Inserts



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(NULL);
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM ( SELECT ('a')::int, dml_co_r.b,10 FROM dml_co_s WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT ('a')::int, dml_co_r.b,10 FROM dml_co_s WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test21: Negative test case. INSERT has more expressions than target columns



SELECT COUNT(*) FROM dml_co_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_co_r.* FROM dml_co_r, dml_co_s WHERE dml_co_s.a = dml_co_r.a GROUP BY dml_co_r.a, dml_co_r.b, dml_co_r.c)foo;
INSERT INTO dml_co_s SELECT COUNT(*) as a, dml_co_r.* FROM dml_co_r, dml_co_s WHERE dml_co_s.a = dml_co_r.a GROUP BY dml_co_r.a, dml_co_r.b, dml_co_r.c;
SELECT COUNT(*) FROM dml_co_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Multiple constant tuple Inserts



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Insert with generate_series



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(generate_series(1,10), generate_series(1,100), 'text');
SELECT COUNT(*) FROM dml_co_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_co_r WHERE c ='text' ORDER BY 1;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test6: Insert with generate_series



SELECT COUNT(*) FROM dml_co_r;
TRUNCATE TABLE dml_co_r;
INSERT INTO dml_co_r SELECT generate_series(1,10);
SELECT * FROM dml_co_r ORDER BY 1;
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test7: Insert with generate_series



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT generate_series(1,10), generate_series(1,10),'text';
SELECT COUNT(*) FROM dml_co_r WHERE c ='text';
SELECT COUNT(*) FROM dml_co_r;
	
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Insert with generate_series



SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT * from generate_series(1,10);
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb

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
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table



SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT dml_co_r.* FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s  WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Insert data that satisfy the check constraints 



SELECT COUNT(*) FROM dml_heap_check_s;
SELECT COUNT(*) FROM (SELECT dml_heap_check_s.a, dml_heap_check_s.b, 'text', dml_heap_check_s.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.b )foo;
INSERT INTO dml_heap_check_s SELECT dml_heap_check_s.a, dml_heap_check_s.b, 'text', dml_heap_check_s.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.b ;
SELECT COUNT(*) FROM dml_heap_check_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test2: Negative test - Insert default value violates check constraint



SELECT COUNT(*) FROM dml_heap_check_p;
INSERT INTO dml_heap_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_heap_check_p;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test3: Negative test - Insert default value violates NOT NULL constraint



SELECT COUNT(*) FROM dml_heap_check_s;
INSERT INTO dml_heap_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_heap_check_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint



SELECT COUNT(*) FROM dml_heap_check_r;
SELECT COUNT(*) FROM (SELECT dml_heap_check_r.a + 110 , dml_heap_check_r.b, dml_heap_check_r.c, dml_heap_check_r.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a)foo;
INSERT INTO dml_heap_check_r SELECT dml_heap_check_r.a + 110 , dml_heap_check_r.b, dml_heap_check_r.c, dml_heap_check_r.d FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a;
SELECT COUNT(*) FROM dml_heap_check_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test5: Insert with joins where the result tuples violate violates multiple check constraints



SELECT COUNT(*) FROM dml_heap_check_r;
SELECT COUNT(*) FROM (SELECT dml_heap_check_r.a + 110 , 0, dml_heap_check_r.c, NULL FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a)foo;
INSERT INTO dml_heap_check_r SELECT dml_heap_check_r.a + 110 , 0, dml_heap_check_r.c, NULL FROM dml_heap_check_r, dml_heap_check_s WHERE dml_heap_check_r.a = dml_heap_check_s.a;
SELECT COUNT(*) FROM dml_heap_check_r;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Update data that satisfy the check constraints 



SELECT SUM(d) FROM dml_heap_check_s;
UPDATE dml_heap_check_s SET d = dml_heap_check_s.d * 1 FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.b;
SELECT SUM(d) FROM dml_heap_check_s;
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test2: Negative test: Update data that does not satisfy the check constraints 



UPDATE dml_heap_check_s SET a = 100 + dml_heap_check_s.a FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.a ; 
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test3: Negative test - Update violates check constraint(not NULL constraint)



UPDATE dml_heap_check_s SET b = NULL FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.b; 
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test4: Negative test - Update moving tuple across partition .also violates the check constraint



UPDATE dml_heap_check_s SET a = 110 + dml_heap_check_s.a FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.a ; 
-- start_ignore
-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb


DROP TABLE IF EXISTS dml_heap_check_p;
DROP TABLE IF EXISTS dml_heap_check_r;
DROP TABLE IF EXISTS dml_heap_check_s;

CREATE TABLE dml_heap_check_r (
	a int default 100 CHECK( a between 1 and 105),
	b float8 CONSTRAINT rcheck_b CHECK( b <> 0.00 and b IS NOT NULL),
	c text,
	d numeric NOT NULL)
DISTRIBUTED BY (a)
partition by list(b) (
	partition one values (1.0,2.0,3.0,4.0,5.0),
	partition two values(6.0,7.0,8.0,9.0,10.00),
	default partition def
);

CREATE TABLE dml_heap_check_s (
	a int CONSTRAINT scheck_a NOT NULL CHECK(a >= 0 and a < 110),
	b int CONSTRAINT scheck_b CHECK( b is not null),
	c text ,
	d numeric CHECK (d - 1 <> 17) )
DISTRIBUTED BY (b)
partition by range(a)
(
        start(1) end(102) every(10),
        default partition def
);

CREATE TABLE dml_heap_check_p (
	a int NOT NULL CONSTRAINT p_check_a CHECK( a <= 100) ,
	b text DEFAULT 'defval',
	c text ,
	d float8 CONSTRAINT p_check_d CHECK( d <= 100.00),
	CHECK ( b = c )
) DISTRIBUTED BY (a,b);

INSERT INTO dml_heap_check_r SELECT i, i * 3.33,'r', i % 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_r VALUES(DEFAULT,1.00,'rn',0),(NULL,2.00,'rn',0),(NULL,3.00,'rn',0),(NULL,4.00,'rn',0),(DEFAULT,5.00,'rn',0);
INSERT INTO dml_heap_check_s SELECT i, i * 3,'s', i / 6 FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_s VALUES(1,1,'sn',NULL),(2,2,'sn',NULL),(3,3,'sn',NULL),(4,4,'sn',NULL),(5,5,'sn',NULL);
INSERT INTO dml_heap_check_p SELECT i, 'p','p', i FROM generate_series(1,100)i;
INSERT INTO dml_heap_check_p VALUES(1,'pn','pn',NULL),(2,'pn','pn',NULL),(3,'pn','pn',NULL),(4,'pn','pn',NULL),(5,'pn','pn',NULL);
-- end_ignore
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test5: Negative test - Update violates multiple check constraint



UPDATE dml_heap_check_s SET a = 110 + dml_heap_check_s.a, b = NULL  FROM dml_heap_check_r WHERE dml_heap_check_r.b = dml_heap_check_s.b;
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
-- @description test1: Delete from table



SELECT COUNT(*) FROM dml_heap_s; 
DELETE FROM dml_heap_s;
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
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Delete with join in using



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT dml_heap_r.a FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a)foo WHERE dml_heap_r.a = foo.a;
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
-- @description test11: Delete with join in USING (Delete all rows )



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT 1)foo; 
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
-- @description test12: Delete with join in using



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT 1 as t) foo WHERE foo.t = dml_heap_r.a; 
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
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test13: Delete with multiple joins
-- @execute_all_plans True



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s,dml_heap_p WHERE dml_heap_r.a = dml_heap_s.b and dml_heap_r.b = dml_heap_p.a;
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
-- @description test14: Delete on table with composite distcol



SELECT COUNT(*) FROM dml_heap_p;
DELETE FROM dml_heap_p USING dml_heap_r WHERE dml_heap_p.b::int = dml_heap_r.b::int and dml_heap_p.a = dml_heap_r.a;
SELECT COUNT(*) FROM dml_heap_p;
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
-- @description test15: Delete with PREPARE plan



SELECT COUNT(*) FROM (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b) foo;
SELECT COUNT(*) FROM dml_heap_r;
PREPARE plan_del as DELETE FROM dml_heap_r WHERE b in (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b);
EXECUTE plan_del;
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
-- @description test16: Delete with PREPARE plan



SELECT COUNT(*) FROM (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a)foo;
SELECT COUNT(*) FROM dml_heap_s;
PREPARE plan_del_2 as DELETE FROM dml_heap_s WHERE b in (SELECT dml_heap_r.b FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a);
EXECUTE plan_del_2;
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
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test17: Delete with sub-query



SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s WHERE a = (SELECT dml_heap_r.a FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a);
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
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Delete with predicate



SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM dml_heap_s WHERE a > 100;
DELETE FROM dml_heap_s WHERE a > 100;
SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM dml_heap_s WHERE a > 100;
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
-- @description test3: Delete with predicate



SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM dml_heap_s WHERE a is NULL;
DELETE FROM dml_heap_s WHERE a is NULL; 
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
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test4: Delete with generate_series



SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s USING generate_series(1,10);
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
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Delete with join on distcol



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
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
-- @description test6: Delete with join on non-distribution column



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b;
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
-- @description test7: Delete with join on non-distribution column



SELECT COUNT(*) FROM dml_heap_s;
DELETE FROM dml_heap_s USING dml_heap_r WHERE dml_heap_s.a = dml_heap_r.a;
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
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test8: Delete and using  



SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s; 
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
-- @description test9: Delete and using (with no rows)



TRUNCATE TABLE dml_heap_s;
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s; 
SELECT COUNT(*) FROM dml_heap_r;
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
-- @description test1: Delete from table

SELECT COUNT(*) FROM dml_heap_pt_s; 
DELETE FROM dml_heap_pt_s;
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
-- @description test10: Delete with join in using


SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT dml_heap_pt_r.a FROM dml_heap_pt_r, dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a)foo WHERE dml_heap_pt_r.a = foo.a;
SELECT COUNT(*) FROM dml_heap_pt_r;

-- start_ignore
drop schema functional_76_150 cascade;
-- end_ignore
