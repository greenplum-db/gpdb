
-- start_ignore
create schema functional_301_361;
set search_path to functional_301_361;
-- end_ignore
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
-- @description union_test1: INSERT and INTERSECT operation




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.a, dml_union_r.b, dml_union_r.c, dml_union_r.d FROM dml_union_r INTERSECT SELECT dml_union_s.* FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT dml_union_r.a, dml_union_r.b, dml_union_r.c, dml_union_r.d FROM dml_union_r INTERSECT SELECT dml_union_s.* FROM dml_union_s;
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
-- @description union_test10:  INSERT and EXCEPT operation




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM dml_union_r EXCEPT ALL SELECT dml_union_s.* FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT * FROM dml_union_r EXCEPT ALL SELECT dml_union_s.* FROM dml_union_s;
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
-- @description union_test12: EXCEPT with generate_series




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT generate_series(1,10) EXCEPT ALL SELECT generate_series(1,10))foo;
INSERT INTO dml_union_r SELECT generate_series(1,10) EXCEPT ALL SELECT generate_series(1,10);
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
-- @description union_test13: EXCEPT with predicate




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM (SELECT * FROM dml_union_r EXCEPT ALL SELECT * FROM dml_union_s) foo WHERE c='text')bar;
INSERT INTO dml_union_r SELECT * FROM (SELECT * FROM dml_union_r EXCEPT ALL SELECT * FROM dml_union_s) foo WHERE c='text';
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
-- @description union_test14: EXCEPT with predicate (returns 0 rows)




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM (SELECT * FROM dml_union_r EXCEPT SELECT * FROM dml_union_s) foo WHERE c='s')bar;
INSERT INTO dml_union_r SELECT * FROM (SELECT * FROM dml_union_r EXCEPT SELECT * FROM dml_union_s) foo WHERE c='s';
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
-- @description union_test15: EXCEPT with constants




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.a, dml_union_r.b,'A' as c ,0 as d FROM dml_union_r EXCEPT ALL SELECT dml_union_s.a, dml_union_s.b,'A' as C,0 as d FROM dml_union_s)foo;
INSERT INTO dml_union_r (SELECT dml_union_r.a, dml_union_r.b,'A' as c ,0 as d FROM dml_union_r EXCEPT ALL SELECT dml_union_s.a, dml_union_s.b,'A' as C,0 as d FROM dml_union_s);
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
-- @description union_test16: EXCEPT with distinct




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT distinct a,b,c,d FROM dml_union_r EXCEPT SELECT distinct a,b,c,d FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT distinct a,b,c,d FROM dml_union_r EXCEPT SELECT distinct a,b,c,d FROM dml_union_s;
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
-- @description union_test17: EXCEPT with distinct




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT distinct a,b,c,d FROM dml_union_r EXCEPT ALL SELECT distinct a,b,c,d FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT distinct a,b,c,d FROM dml_union_r EXCEPT ALL SELECT distinct a,b,c,d FROM dml_union_s;
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
-- @description union_test18: INSERT and UNION operation




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.a, dml_union_r.b, dml_union_r.c, dml_union_r.d FROM dml_union_r UNION SELECT dml_union_s.* FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT dml_union_r.a, dml_union_r.b, dml_union_r.c, dml_union_r.d FROM dml_union_r UNION SELECT dml_union_s.* FROM dml_union_s;
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
-- @description union_test19: INSERT and UNION operation




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.* FROM dml_union_r UNION All SELECT * FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT dml_union_r.* FROM dml_union_r UNION All SELECT * FROM dml_union_s;
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
-- @description union_test2: INSERT and INTERSECT operation




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.* FROM dml_union_r INTERSECT ALL SELECT dml_union_s.a, dml_union_s.b, dml_union_s.c, dml_union_s.d FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT dml_union_r.* FROM dml_union_r INTERSECT ALL SELECT dml_union_s.a, dml_union_s.b, dml_union_s.c, dml_union_s.d FROM dml_union_s;
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
-- @description union_test20: UNION with generate_series




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT generate_series(1,10) UNION SELECT generate_series(1,10))foo;
INSERT INTO dml_union_r SELECT generate_series(1,10) UNION SELECT generate_series(1,10);
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
-- @description union_test21: UNION with generate_series




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT generate_series(1,10) UNION ALL SELECT generate_series(1,10))foo;
INSERT INTO dml_union_r SELECT generate_series(1,10) UNION ALL SELECT generate_series(1,10);
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
-- @description union_test22: UNION with limit




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM dml_union_r UNION ALL SELECT * FROM dml_union_s ORDER BY 1,2,3,4) foo LIMIT 10;
SELECT COUNT(*) FROM dml_union_r;
INSERT INTO dml_union_r SELECT * FROM (SELECT * FROM dml_union_r UNION ALL SELECT * FROM dml_union_s ORDER BY 1,2,3,4) foo LIMIT 10;
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
-- @description union_test23: UNION with dml_union_sub-query in SELECT




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT NULL,(SELECT NULL f1 FROM dml_union_r UNION SELECT NULL f1 FROM dml_union_s)::int, 'nullval',NULL)foo;
INSERT INTO dml_union_r SELECT NULL,(SELECT NULL f1 FROM dml_union_r UNION SELECT NULL f1 FROM dml_union_s)::int, 'nullval',NULL;
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
-- @description union_test24: UNION with exists 




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT AVG(a),10,'avg',10 FROM dml_union_r WHERE exists (SELECT a FROM dml_union_r UNION ALL SELECT b FROM dml_union_s))foo;
INSERT INTO dml_union_r SELECT AVG(a),10,'avg',10 FROM dml_union_r WHERE exists (SELECT a FROM dml_union_r UNION ALL SELECT b FROM dml_union_s);
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
-- @description union_test25: UNION with DISTINCT




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT distinct a,b,c,d FROM dml_union_r UNION SELECT distinct a,b,c,d FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT distinct a,b,c,d FROM dml_union_r UNION SELECT distinct a,b,c,d FROM dml_union_s;
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
-- @description union_test26: UNION with AGGREGATE




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM (SELECT AVG(a) as a FROM dml_union_r UNION SELECT AVG(b) as a FROM dml_union_s) foo)bar;
INSERT INTO dml_union_r SELECT * FROM (SELECT AVG(a) as a FROM dml_union_r UNION SELECT AVG(b) as a FROM dml_union_s) foo;
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
-- @gpopt 1.532
-- @description union_test27: Negative tests VIOLATES NULL VALUE CONSTRAINT 




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM ( SELECT * FROM (SELECT * FROM dml_union_r EXCEPT SELECT * FROM dml_union_s ) foo WHERE c='text')bar;
INSERT INTO dml_union_s SELECT * FROM (SELECT * FROM dml_union_r EXCEPT SELECT * FROM dml_union_s) foo WHERE c='text';
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
-- @gpopt 1.532
-- @description union_test28: Negative tests MORE THAN ONE ROW RETURNED




SELECT COUNT(*) FROM dml_union_r;
INSERT INTO dml_union_r SELECT (SELECT dml_union_r.d::int FROM dml_union_r INTERSECT SELECT dml_union_s.d FROM dml_union_s ORDER BY 1),1,'newval',1.000;
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
-- @description union_test29: INSERT NON ATOMICS with union/intersect/except




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.* FROM dml_union_r INTERSECT (SELECT dml_union_r.* FROM dml_union_r UNION ALL SELECT dml_union_s.* FROM dml_union_s) EXCEPT SELECT dml_union_s.* FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT dml_union_r.* FROM dml_union_r INTERSECT (SELECT dml_union_r.* FROM dml_union_r UNION ALL SELECT dml_union_s.* FROM dml_union_s) EXCEPT SELECT dml_union_s.* FROM dml_union_s;
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
-- @description union_test3: INTERSECT with generate_series




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT generate_series(1,10) INTERSECT SELECT generate_series(1,100))foo;
INSERT INTO dml_union_r SELECT generate_series(1,10) INTERSECT SELECT generate_series(1,100);
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
-- @description union_test30: INSERT NON ATOMICS with union/intersect/except




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT c,c+1,case when c = 1 then 'r' else 's' end,c+1 FROM (    SELECT COUNT(distinct c) c FROM (SELECT f1, f2 , COUNT(*) c FROM (SELECT 10 f1, 'r' f2 FROM dml_union_r UNION SELECT 40 f1, 's' f2 FROM dml_union_r UNION SELECT a, c FROM dml_union_r INTERSECT SELECT a, c FROM dml_union_s ORDER BY 1) foo group by f1,f2) foo)foo)bar;
INSERT INTO dml_union_r SELECT c,c+1,case when c = 1 then 'r' else 's' end,c+1 FROM (SELECT COUNT(distinct c) c FROM (SELECT f1, f2 , COUNT(*) c FROM (SELECT 10 f1, 'r' f2 FROM dml_union_r UNION SELECT 40 f1, 's' f2 FROM dml_union_r UNION SELECT a, c FROM dml_union_r INTERSECT SELECT a, c FROM dml_union_s ORDER BY 1) foo group by f1,f2) foo)foo;
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
-- @description union_test31: INSERT NON ATOMICS with union/intersect/except




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM dml_union_r WHERE a in (SELECT dml_union_r.d::int FROM dml_union_r INTERSECT SELECT dml_union_s.d FROM dml_union_s ORDER BY 1) UNION SELECT * FROM dml_union_s)bar;
INSERT INTO dml_union_r SELECT * FROM dml_union_r WHERE a in (SELECT dml_union_r.d::int FROM dml_union_r INTERSECT SELECT dml_union_s.d FROM dml_union_s ORDER BY 1) UNION SELECT * FROM dml_union_s;
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
-- @description union_test4: INTERSECT with generate_series




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT generate_series(1,10) INTERSECT ALL  SELECT generate_series(1,100))foo;
INSERT INTO dml_union_r SELECT generate_series(1,10) INTERSECT ALL  SELECT generate_series(1,100);
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
-- @description union_test5: INTERSECT with constants




SELECT COUNT(*) FROM dml_union_s;
SELECT COUNT(*) FROM (SELECT dml_union_r.a, dml_union_r.b,'A' as c, 0 as d FROM dml_union_r INTERSECT SELECT dml_union_s.a, dml_union_s.b,'A' as C,0 as d FROM dml_union_s)foo;
INSERT INTO dml_union_s (SELECT dml_union_r.a, dml_union_r.b,'A' as c, 0 as d FROM dml_union_r INTERSECT SELECT dml_union_s.a, dml_union_s.b,'A' as C,0 as d FROM dml_union_s);
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
-- @description union_test6: INTERSECT with constants




SELECT COUNT(*) FROM dml_union_s;
SELECT COUNT(*) FROM (SELECT dml_union_r.a, dml_union_r.b,'A' as c ,0 as d FROM dml_union_r INTERSECT ALL SELECT dml_union_s.a, dml_union_s.b,'A' as C,0 as d FROM dml_union_s)foo;
INSERT INTO dml_union_s (SELECT dml_union_r.a, dml_union_r.b,'A' as c ,0 as d FROM dml_union_r INTERSECT ALL SELECT dml_union_s.a, dml_union_s.b,'A' as C,0 as d FROM dml_union_s);
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
-- @description union_test7: INTERSECT with DISTINCT




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT distinct a,b,c,d FROM dml_union_r INTERSECT SELECT distinct a,b,c,d FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT distinct a,b,c,d FROM dml_union_r INTERSECT SELECT distinct a,b,c,d FROM dml_union_s;
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
-- @description union_test8: INTERSECT with DISTINCT




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT distinct a,b,c,d FROM dml_union_r INTERSECT ALL SELECT distinct a,b,c,d FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT distinct a,b,c,d FROM dml_union_r INTERSECT ALL SELECT distinct a,b,c,d FROM dml_union_s;
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
-- @description union_test9:  INSERT and EXCEPT operation




SELECT COUNT(*) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT dml_union_r.a, dml_union_r.b, dml_union_r.c, dml_union_r.d FROM dml_union_r EXCEPT SELECT * FROM dml_union_s)foo;
INSERT INTO dml_union_r SELECT dml_union_r.a, dml_union_r.b, dml_union_r.c, dml_union_r.d FROM dml_union_r EXCEPT SELECT * FROM dml_union_s;
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
-- @description union_update_test1: Update distribution column with UNION




SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT a FROM dml_union_r UNION SELECT a FROM dml_union_s ORDER BY 1 LIMIT 1;
UPDATE dml_union_r SET a = (SELECT a FROM dml_union_r UNION SELECT a FROM dml_union_s ORDER BY 1 LIMIT 1);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
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
-- @description union_update_test10: NULL values to distribution key





SELECT COUNT(DISTINCT(a)) FROM dml_union_r; 
UPDATE dml_union_r SET a = (SELECT NULL EXCEPT SELECT NULL)::int;
SELECT COUNT(DISTINCT(a)) FROM dml_union_r; 
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
-- @description union_update_test11: NULL values to text




SELECT COUNT(DISTINCT(c)) FROM dml_union_r; 
UPDATE dml_union_r SET c = (SELECT NULL EXCEPT ALL SELECT NULL);
SELECT COUNT(DISTINCT(c)) FROM dml_union_r; 

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
-- @description union_update_test12: Update partition key to NULL values when default partition present




SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 
UPDATE dml_union_s SET d = (SELECT NULL UNION SELECT NULL)::numeric;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 

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
-- @description union_update_test13: Update partition key to NULL values when default partition present




SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 
UPDATE dml_union_s SET d = (SELECT NULL INTERSECT SELECT NULL)::numeric;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 

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
-- @description union_update_test14: Update partition key to NULL values when default partition present




SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 
UPDATE dml_union_s SET d = (SELECT NULL INTERSECT ALL SELECT NULL)::numeric;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 

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
-- @description union_update_test15: Update partition key to NULL values when default partition present




SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 
UPDATE dml_union_s SET d = (SELECT NULL EXCEPT SELECT NULL)::numeric;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 
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
-- @description union_update_test16: Update partition key to NULL values when default partition present




SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 
UPDATE dml_union_s SET d = (SELECT NULL EXCEPT ALL SELECT NULL)::numeric;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s; 

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
-- @description union_update_test17: Update partition key to constant value with UNION/INTERSECT/EXCEPT within dml_union_sub-query




SELECT COUNT(*) FROM dml_union_r WHERE d = 20000; 
SELECT COUNT(*) FROM (SELECT a FROM dml_union_r UNION ALL SELECT a FROM dml_union_s)foo;
UPDATE dml_union_r SET d = 20000 WHERE a in (SELECT a FROM dml_union_r UNION ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r WHERE d = 20000; 
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
-- @description union_update_test18: Update partition key to constant value with UNION/INTERSECT/EXCEPT within dml_union_sub-query




SELECT COUNT(*) FROM dml_union_r WHERE d = 20000; 
SELECT COUNT(*) FROM (SELECT a FROM dml_union_r INTERSECT ALL SELECT a FROM dml_union_s)foo;
UPDATE dml_union_r SET d = 20000 WHERE a in (SELECT a FROM dml_union_r INTERSECT ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r WHERE d = 20000; 
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
-- @description union_update_test19: Update partition key to constant value with UNION/INTERSECT/EXCEPT within dml_union_sub-query




SELECT COUNT(*) FROM dml_union_r WHERE d = 20000; 
SELECT COUNT(*) FROM (SELECT a FROM dml_union_r EXCEPT ALL SELECT a FROM dml_union_s)foo;
UPDATE dml_union_r SET d = 20000 WHERE a in (SELECT a FROM dml_union_r EXCEPT ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r WHERE d = 20000; 
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
-- @description union_update_test2: Update distribution column with UNION



SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
UPDATE dml_union_r SET a = (SELECT a FROM dml_union_r UNION ALL SELECT a FROM dml_union_s ORDER BY 1 LIMIT 1);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT DISTINCT(a) FROM dml_union_r;
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
-- @description union_update_test20:  UNION/INTERSECT/EXCEPT within dml_union_sub-query



SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
UPDATE dml_union_r SET a = dml_union_s.a FROM dml_union_s WHERE dml_union_r.b in (SELECT b FROM dml_union_r UNION SELECT b FROM dml_union_s);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
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
-- @description union_update_test21:  UNION/INTERSECT/EXCEPT within dml_union_sub-query




SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
UPDATE dml_union_r SET a = dml_union_r.a WHERE b in (SELECT b FROM dml_union_r INTERSECT SELECT b FROM dml_union_s);
SELECT DISTINCT(a) FROM dml_union_r;
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
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
-- @description union_update_test22:  UNION/INTERSECT/EXCEPT within dml_union_sub-query



SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
UPDATE dml_union_r SET a = dml_union_r.a WHERE b in (SELECT b FROM dml_union_r EXCEPT SELECT b FROM dml_union_s);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT DISTINCT(a) FROM dml_union_r;
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
-- @description union_update_test23: Update distribution column to constant value with UNION/INTERSECT/EXCEPT within dml_union_sub-query



SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT COUNT(*) FROM dml_union_r WHERE a = 0;
UPDATE dml_union_r SET a = 0 WHERE a in (SELECT a FROM dml_union_r UNION ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r WHERE a = 0;
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
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
-- @description union_update_test24: Update distribution column to constant value with UNION/INTERSECT/EXCEPT within dml_union_sub-query



SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT COUNT(*) FROM dml_union_r WHERE a = 0;
UPDATE dml_union_r SET a = 0 WHERE a in (SELECT a FROM dml_union_r INTERSECT ALL SELECT a FROM dml_union_s);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT COUNT(*) FROM dml_union_r WHERE a = 0;

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
-- @description union_update_test25: Update distribution column to constant value with UNION/INTERSECT/EXCEPT within dml_union_sub-query



SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT COUNT(*) FROM dml_union_r WHERE a = 0;
UPDATE dml_union_r SET a = 0 WHERE a in (SELECT a FROM dml_union_r EXCEPT ALL SELECT a FROM dml_union_s);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
SELECT COUNT(*) FROM dml_union_r WHERE a = 0;
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
-- @description union_update_test26: Negative Tests Update the partition key to an out of dml_union_range value with no default partition




ALTER TABLE dml_union_s drop default partition;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s;
UPDATE dml_union_s SET d = (SELECT NULL UNION SELECT NULL)::numeric;
SELECT DISTINCT(d) FROM dml_union_s;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s;
ALTER TABLE dml_union_s ADD default partition def;

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
-- @description union_update_test27: Negative Tests Update the partition key to an out of range value with no default partition




ALTER TABLE dml_union_s drop default partition;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s;
UPDATE dml_union_s SET d = (SELECT NULL INTERSECT SELECT NULL)::numeric; 
SELECT DISTINCT(d) FROM dml_union_s;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s;
ALTER TABLE dml_union_s ADD default partition def;

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
-- @description union_update_test28: Negative Tests Update the partition key to an out of dml_union_range value with no default partition




ALTER TABLE dml_union_s drop default partition;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s;
UPDATE dml_union_s SET d = (SELECT NULL EXCEPT SELECT NULL)::numeric; 
SELECT DISTINCT(d) FROM dml_union_s;
SELECT COUNT(DISTINCT(d)) FROM dml_union_s;
ALTER TABLE dml_union_s ADD default partition def;

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
-- @gpopt 1.532
-- @description union_update_test29: Negative Tests  UPDATE violates the CHECK constraint on the column




SELECT COUNT(DISTINCT(b)) FROM dml_union_s;
UPDATE dml_union_s SET b = (SELECT NULL UNION SELECT NULL)::numeric;

SELECT COUNT(DISTINCT(b)) FROM dml_union_s;
SELECT DISTINCT(b) FROM dml_union_s;

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
-- @description union_update_test3: Update distribution column with INTERSECT




SELECT COUNT(*) FROM dml_union_r WHERE a = 1;
SELECT COUNT(*) FROM (SELECT * FROM (SELECT a FROM dml_union_r order by a limit 1) foo INTERSECT SELECT a FROM dml_union_s)bar;
UPDATE dml_union_r SET a = ( SELECT * FROM (SELECT a FROM dml_union_r order by a limit 1) foo INTERSECT SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r WHERE a = 1;
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
-- @gpopt 1.532
-- @description union_update_test30: Negative Tests  more than one row returned by a sub-query used as an expression




SELECT COUNT(DISTINCT(a)) FROM dml_union_r;
UPDATE dml_union_r SET a = ( SELECT a FROM dml_union_r UNION ALL SELECT a FROM dml_union_s);
SELECT COUNT(DISTINCT(a)) FROM dml_union_r;

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
-- @gpopt 1.532
-- @description union_update_test31: Negative Tests  more than one row returned by a sub-query used as an expression




UPDATE dml_union_r SET b = ( SELECT a FROM dml_union_r EXCEPT ALL SELECT a FROM dml_union_s);

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
-- @description union_update_test4: Update distribution column with INTERSECT




SELECT COUNT(*) FROM dml_union_r WHERE a = 1;
SELECT COUNT(*) FROM (SELECT * FROM (SELECT a FROM dml_union_r ORDER BY 1 limit 1) foo INTERSECT ALL SELECT a FROM dml_union_s)bar; 
UPDATE dml_union_r SET a = ( SELECT * FROM (SELECT a FROM dml_union_r ORDER BY 1 limit 1) foo INTERSECT ALL SELECT a FROM dml_union_s);
SELECT COUNT(*) FROM dml_union_r WHERE a = 1;

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
-- @description union_update_test5: Update distribution column with EXCEPT




SELECT SUM(a) FROM dml_union_r;
SELECT COUNT(*) FROM (SELECT * FROM (SELECT a FROM dml_union_r limit 1) foo EXCEPT SELECT a FROM dml_union_s)bar;
UPDATE dml_union_r SET a = ( SELECT * FROM (SELECT a FROM dml_union_r limit 1) foo EXCEPT SELECT a FROM dml_union_s);
SELECT SUM(a) FROM dml_union_r;
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
-- @description union_update_test6: Update distribution column with EXCEPT




UPDATE dml_union_r SET a = ( SELECT * FROM (SELECT a FROM dml_union_r limit 1) foo EXCEPT ALL SELECT a FROM dml_union_s);
SELECT DISTINCT(a) FROM dml_union_r;
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
-- @description union_update_test7: NULL values to distribution key




UPDATE dml_union_r SET a = (SELECT NULL UNION SELECT NULL)::int;
SELECT DISTINCT(a) FROM dml_union_r;
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
-- @description union_update_test8: NULL values to distribution key




UPDATE dml_union_r SET a = (SELECT NULL INTERSECT SELECT NULL)::int;
SELECT DISTINCT(a) FROM dml_union_r;

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
-- @description union_update_test9: NULL values to distribution key

UPDATE dml_union_r SET a = (SELECT NULL INTERSECT ALL SELECT NULL)::int;
SELECT DISTINCT(a) FROM dml_union_r;

-- start_ignore
drop schema functional_301_361 cascade;
-- end_ignore
